// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/attachments/attachment_uploader_impl.h"

#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/sys_byteorder.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/sync/model/attachments/attachment.h"
#include "components/sync/protocol/sync.pb.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"

namespace {

const char kContentType[] = "application/octet-stream";
const char kAttachments[] = "attachments/";
const char kSyncStoreBirthday[] = "X-Sync-Store-Birthday";
const char kSyncDataTypeId[] = "X-Sync-Data-Type-Id";

}  // namespace

namespace syncer {

// Encapsulates all the state associated with a single upload.
class AttachmentUploaderImpl::UploadState : public net::URLFetcherDelegate,
                                            public OAuth2TokenService::Consumer,
                                            public base::NonThreadSafe {
 public:
  // Construct an UploadState.
  //
  // UploadState encapsulates the state associated with a single upload.  When
  // the upload completes, the UploadState object becomes "stopped".
  //
  // |owner| is a pointer to the object that owns this UploadState.  Upon
  // completion this object will PostTask to owner's OnUploadStateStopped
  // method.
  UploadState(
      const GURL& upload_url,
      const scoped_refptr<net::URLRequestContextGetter>&
          url_request_context_getter,
      const Attachment& attachment,
      const UploadCallback& user_callback,
      const std::string& account_id,
      const OAuth2TokenService::ScopeSet& scopes,
      OAuth2TokenServiceRequest::TokenServiceProvider* token_service_provider,
      const std::string& raw_store_birthday,
      const base::WeakPtr<AttachmentUploaderImpl>& owner,
      ModelType model_type);

  ~UploadState() override;

  // Returns true if this object is stopped.  Once stopped, this object is
  // effectively dead and can be destroyed.
  bool IsStopped() const;

  // Add |user_callback| to the list of callbacks to be invoked when this upload
  // completed.
  //
  // It is an error to call |AddUserCallback| on a stopped UploadState (see
  // |IsStopped|).
  void AddUserCallback(const UploadCallback& user_callback);

  // Return the Attachment this object is uploading.
  const Attachment& GetAttachment();

  // URLFetcher implementation.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // OAuth2TokenService::Consumer.
  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

 private:
  using UploadCallbackList = std::vector<UploadCallback>;

  void GetToken();

  void StopAndReportResult(const UploadResult& result,
                           const AttachmentId& attachment_id);

  bool is_stopped_;
  GURL upload_url_;
  const scoped_refptr<net::URLRequestContextGetter>&
      url_request_context_getter_;
  Attachment attachment_;
  UploadCallbackList user_callbacks_;
  std::unique_ptr<net::URLFetcher> fetcher_;
  std::string account_id_;
  OAuth2TokenService::ScopeSet scopes_;
  std::string access_token_;
  std::string raw_store_birthday_;
  OAuth2TokenServiceRequest::TokenServiceProvider* token_service_provider_;
  // Pointer to the AttachmentUploaderImpl that owns this object.
  base::WeakPtr<AttachmentUploaderImpl> owner_;
  std::unique_ptr<OAuth2TokenServiceRequest> access_token_request_;
  ModelType model_type_;

  DISALLOW_COPY_AND_ASSIGN(UploadState);
};

AttachmentUploaderImpl::UploadState::UploadState(
    const GURL& upload_url,
    const scoped_refptr<net::URLRequestContextGetter>&
        url_request_context_getter,
    const Attachment& attachment,
    const UploadCallback& user_callback,
    const std::string& account_id,
    const OAuth2TokenService::ScopeSet& scopes,
    OAuth2TokenServiceRequest::TokenServiceProvider* token_service_provider,
    const std::string& raw_store_birthday,
    const base::WeakPtr<AttachmentUploaderImpl>& owner,
    ModelType model_type)
    : OAuth2TokenService::Consumer("attachment-uploader-impl"),
      is_stopped_(false),
      upload_url_(upload_url),
      url_request_context_getter_(url_request_context_getter),
      attachment_(attachment),
      user_callbacks_(1, user_callback),
      account_id_(account_id),
      scopes_(scopes),
      raw_store_birthday_(raw_store_birthday),
      token_service_provider_(token_service_provider),
      owner_(owner),
      model_type_(model_type) {
  DCHECK(upload_url_.is_valid());
  DCHECK(url_request_context_getter_.get());
  DCHECK(!account_id_.empty());
  DCHECK(!scopes_.empty());
  DCHECK(token_service_provider_);
  DCHECK(!raw_store_birthday_.empty());
  GetToken();
}

AttachmentUploaderImpl::UploadState::~UploadState() {}

bool AttachmentUploaderImpl::UploadState::IsStopped() const {
  DCHECK(CalledOnValidThread());
  return is_stopped_;
}

void AttachmentUploaderImpl::UploadState::AddUserCallback(
    const UploadCallback& user_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!is_stopped_);
  user_callbacks_.push_back(user_callback);
}

const Attachment& AttachmentUploaderImpl::UploadState::GetAttachment() {
  DCHECK(CalledOnValidThread());
  return attachment_;
}

void AttachmentUploaderImpl::UploadState::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(CalledOnValidThread());
  if (is_stopped_) {
    return;
  }

  UploadResult result = UPLOAD_TRANSIENT_ERROR;
  AttachmentId attachment_id = attachment_.GetId();
  net::URLRequestStatus status = source->GetStatus();
  const int response_code = source->GetResponseCode();
  UMA_HISTOGRAM_SPARSE_SLOWLY(
      "Sync.Attachments.UploadResponseCode",
      status.is_success() ? response_code : status.error());
  if (response_code == net::HTTP_OK) {
    result = UPLOAD_SUCCESS;
  } else if (response_code == net::HTTP_UNAUTHORIZED) {
    // Server tells us we've got a bad token so invalidate it.
    OAuth2TokenServiceRequest::InvalidateToken(
        token_service_provider_, account_id_, scopes_, access_token_);
    // Fail the request, but indicate that it may be successful if retried.
    result = UPLOAD_TRANSIENT_ERROR;
  } else if (response_code == net::HTTP_FORBIDDEN) {
    // User is not allowed to use attachments.  Retrying won't help.
    result = UPLOAD_UNSPECIFIED_ERROR;
  } else if (response_code == net::URLFetcher::RESPONSE_CODE_INVALID) {
    result = UPLOAD_TRANSIENT_ERROR;
  }
  StopAndReportResult(result, attachment_id);
}

void AttachmentUploaderImpl::UploadState::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  DCHECK(CalledOnValidThread());
  if (is_stopped_) {
    return;
  }

  DCHECK_EQ(access_token_request_.get(), request);
  access_token_request_.reset();
  access_token_ = access_token;
  fetcher_ = net::URLFetcher::Create(upload_url_, net::URLFetcher::POST, this);
  ConfigureURLFetcherCommon(fetcher_.get(), access_token_, raw_store_birthday_,
                            model_type_, url_request_context_getter_.get());
  data_use_measurement::DataUseUserData::AttachToFetcher(
      fetcher_.get(), data_use_measurement::DataUseUserData::SYNC);
  const uint32_t crc32c = attachment_.GetCrc32c();
  fetcher_->AddExtraRequestHeader(base::StringPrintf(
      "X-Goog-Hash: crc32c=%s", FormatCrc32cHash(crc32c).c_str()));

  // TODO(maniscalco): Is there a better way?  Copying the attachment data into
  // a string feels wrong given how large attachments may be (several MBs).  If
  // we may end up switching from URLFetcher to URLRequest, this copy won't be
  // necessary.
  scoped_refptr<base::RefCountedMemory> memory = attachment_.GetData();
  const std::string upload_content(memory->front_as<char>(), memory->size());
  fetcher_->SetUploadData(kContentType, upload_content);

  fetcher_->Start();
}

void AttachmentUploaderImpl::UploadState::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  DCHECK(CalledOnValidThread());
  if (is_stopped_) {
    return;
  }

  DCHECK_EQ(access_token_request_.get(), request);
  access_token_request_.reset();
  // TODO(maniscalco): We treat this as a transient error, but it may in fact be
  // a very long lived error and require user action.  Consider differentiating
  // between the causes of GetToken failure and act accordingly.  Think about
  // the causes of GetToken failure. Are there (bug 412802).
  StopAndReportResult(UPLOAD_TRANSIENT_ERROR, attachment_.GetId());
}

void AttachmentUploaderImpl::UploadState::GetToken() {
  access_token_request_ = OAuth2TokenServiceRequest::CreateAndStart(
      token_service_provider_, account_id_, scopes_, this);
}

void AttachmentUploaderImpl::UploadState::StopAndReportResult(
    const UploadResult& result,
    const AttachmentId& attachment_id) {
  DCHECK(!is_stopped_);
  is_stopped_ = true;
  UploadCallbackList::const_iterator iter = user_callbacks_.begin();
  UploadCallbackList::const_iterator end = user_callbacks_.end();
  for (; iter != end; ++iter) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(*iter, result, attachment_id));
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&AttachmentUploaderImpl::OnUploadStateStopped,
                            owner_, attachment_id.GetProto().unique_id()));
}

AttachmentUploaderImpl::AttachmentUploaderImpl(
    const GURL& sync_service_url,
    const scoped_refptr<net::URLRequestContextGetter>&
        url_request_context_getter,
    const std::string& account_id,
    const OAuth2TokenService::ScopeSet& scopes,
    const scoped_refptr<OAuth2TokenServiceRequest::TokenServiceProvider>&
        token_service_provider,
    const std::string& store_birthday,
    ModelType model_type)
    : sync_service_url_(sync_service_url),
      url_request_context_getter_(url_request_context_getter),
      account_id_(account_id),
      scopes_(scopes),
      token_service_provider_(token_service_provider),
      raw_store_birthday_(store_birthday),
      model_type_(model_type),
      weak_ptr_factory_(this) {
  DCHECK(CalledOnValidThread());
  DCHECK(!account_id.empty());
  DCHECK(!scopes.empty());
  DCHECK(token_service_provider_.get());
  DCHECK(!raw_store_birthday_.empty());
}

AttachmentUploaderImpl::~AttachmentUploaderImpl() {
  DCHECK(CalledOnValidThread());
}

void AttachmentUploaderImpl::UploadAttachment(const Attachment& attachment,
                                              const UploadCallback& callback) {
  DCHECK(CalledOnValidThread());
  const AttachmentId attachment_id = attachment.GetId();
  const std::string unique_id = attachment_id.GetProto().unique_id();
  DCHECK(!unique_id.empty());
  StateMap::iterator iter = state_map_.find(unique_id);
  if (iter != state_map_.end()) {
    // We have an old upload request for this attachment...
    if (!iter->second->IsStopped()) {
      // "join" to it.
      DCHECK(attachment.GetData()->Equals(
          iter->second->GetAttachment().GetData()));
      iter->second->AddUserCallback(callback);
      return;
    } else {
      // It's stopped so we can't use it.  Delete it.
      state_map_.erase(iter);
    }
  }

  const GURL url = GetURLForAttachmentId(sync_service_url_, attachment_id);
  std::unique_ptr<UploadState> upload_state(new UploadState(
      url, url_request_context_getter_, attachment, callback, account_id_,
      scopes_, token_service_provider_.get(), raw_store_birthday_,
      weak_ptr_factory_.GetWeakPtr(), model_type_));
  state_map_[unique_id] = std::move(upload_state);
}

// Static.
GURL AttachmentUploaderImpl::GetURLForAttachmentId(
    const GURL& sync_service_url,
    const AttachmentId& attachment_id) {
  std::string path = sync_service_url.path();
  if (path.empty() || *path.rbegin() != '/') {
    path += '/';
  }
  path += kAttachments;
  path += attachment_id.GetProto().unique_id();
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  return sync_service_url.ReplaceComponents(replacements);
}

void AttachmentUploaderImpl::OnUploadStateStopped(const UniqueId& unique_id) {
  StateMap::iterator iter = state_map_.find(unique_id);
  // Only erase if stopped.  Because this method is called asynchronously, it's
  // possible that a new request for this same id arrived after the UploadState
  // stopped, but before this method was invoked.  In that case the UploadState
  // in the map might be a new one.
  if (iter != state_map_.end() && iter->second->IsStopped()) {
    state_map_.erase(iter);
  }
}

std::string AttachmentUploaderImpl::FormatCrc32cHash(uint32_t crc32c) {
  const uint32_t crc32c_big_endian = base::HostToNet32(crc32c);
  const base::StringPiece raw(reinterpret_cast<const char*>(&crc32c_big_endian),
                              sizeof(crc32c_big_endian));
  std::string encoded;
  base::Base64Encode(raw, &encoded);
  return encoded;
}

void AttachmentUploaderImpl::ConfigureURLFetcherCommon(
    net::URLFetcher* fetcher,
    const std::string& access_token,
    const std::string& raw_store_birthday,
    ModelType model_type,
    net::URLRequestContextGetter* request_context_getter) {
  DCHECK(request_context_getter);
  DCHECK(fetcher);
  fetcher->SetAutomaticallyRetryOn5xx(false);
  fetcher->SetRequestContext(request_context_getter);
  fetcher->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
                        net::LOAD_DO_NOT_SEND_COOKIES |
                        net::LOAD_DISABLE_CACHE);
  fetcher->AddExtraRequestHeader(base::StringPrintf(
      "%s: Bearer %s", net::HttpRequestHeaders::kAuthorization,
      access_token.c_str()));
  // Encode the birthday.  Birthday is opaque so we assume it could contain
  // anything.  Encode it so that it's safe to pass as an HTTP header value.
  std::string encoded_store_birthday;
  base::Base64UrlEncode(raw_store_birthday,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_store_birthday);
  fetcher->AddExtraRequestHeader(base::StringPrintf(
      "%s: %s", kSyncStoreBirthday, encoded_store_birthday.c_str()));

  // Use field number to pass ModelType because it's stable and we have server
  // code to decode it.
  const int field_number = GetSpecificsFieldNumberFromModelType(model_type);
  fetcher->AddExtraRequestHeader(
      base::StringPrintf("%s: %d", kSyncDataTypeId, field_number));
}

}  // namespace syncer
