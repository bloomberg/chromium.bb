// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata.h"

#include <string>

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "chrome/browser/chromeos/gdata/gdata_operations.h"
#include "chrome/browser/net/browser_url_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"

using content::BrowserThread;

namespace gdata {

namespace {

const char* GetExportFormatParam(DocumentExportFormat format) {
  switch (format) {
    case PNG:
      return "png";
    case HTML:
      return "html";
    case TXT:
      return "txt";
    case DOC:
      return "doc";
    case ODT:
      return "odt";
    case RTF:
      return "rtf";
    case ZIP:
      return "zip";
    case JPEG:
      return "jpeg";
    case SVG:
      return "svg";
    case PPT:
      return "ppt";
    case XLS:
      return "xls";
    case CSV:
      return "csv";
    case ODS:
      return "ods";
    case TSV:
      return "tsv";
    default:
      return "pdf";
  }
}

}  // namespace

//================================ GDataAuthService ============================

void GDataAuthService::Initialize(Profile* profile) {
  profile_ = profile;
  // Get OAuth2 refresh token (if we have any) and register for its updates.
  TokenService* service = TokenServiceFactory::GetForProfile(profile_);
  refresh_token_ = service->GetOAuth2LoginRefreshToken();
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKEN_AVAILABLE,
                 content::Source<TokenService>(service));
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKEN_REQUEST_FAILED,
                 content::Source<TokenService>(service));

  if (!refresh_token_.empty())
    FOR_EACH_OBSERVER(Observer, observers_, OnOAuth2RefreshTokenChanged());
}

GDataAuthService::GDataAuthService()
    : profile_(NULL),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      weak_ptr_bound_to_ui_thread_(weak_ptr_factory_.GetWeakPtr()) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

GDataAuthService::~GDataAuthService() {
}

void GDataAuthService::StartAuthentication(
    GDataOperationRegistry* registry,
    const AuthStatusCallback& callback) {
  scoped_refptr<base::MessageLoopProxy> relay_proxy(
      base::MessageLoopProxy::current());

  if (IsFullyAuthenticated()) {
    relay_proxy->PostTask(FROM_HERE,
         base::Bind(callback, gdata::HTTP_SUCCESS, oauth2_auth_token()));
  } else if (IsPartiallyAuthenticated()) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&GDataAuthService::StartAuthenticationOnUIThread,
                   weak_ptr_bound_to_ui_thread_,
                   registry,
                   relay_proxy,
                   base::Bind(&GDataAuthService::OnAuthCompleted,
                              weak_ptr_bound_to_ui_thread_,
                              relay_proxy,
                              callback)));
  } else {
    relay_proxy->PostTask(FROM_HERE,
        base::Bind(callback, gdata::HTTP_UNAUTHORIZED, std::string()));
  }
}

void GDataAuthService::StartAuthenticationOnUIThread(
    GDataOperationRegistry* registry,
    scoped_refptr<base::MessageLoopProxy> relay_proxy,
    const AuthStatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // We have refresh token, let's gets authenticated.
  (new AuthOperation(registry, profile_,
                     callback, GetOAuth2RefreshToken()))->Start();
}

void GDataAuthService::OnAuthCompleted(
    scoped_refptr<base::MessageLoopProxy> relay_proxy,
    const AuthStatusCallback& callback,
    GDataErrorCode error,
    const std::string& auth_token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error == HTTP_SUCCESS)
    auth_token_ = auth_token;

  // TODO(zelidrag): Add retry, back-off logic when things go wrong here.
  if (!callback.is_null())
    relay_proxy->PostTask(FROM_HERE, base::Bind(callback, error, auth_token));
}

void GDataAuthService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void GDataAuthService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void GDataAuthService::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_TOKEN_AVAILABLE ||
         type == chrome::NOTIFICATION_TOKEN_REQUEST_FAILED);

  TokenService::TokenAvailableDetails* token_details =
      content::Details<TokenService::TokenAvailableDetails>(details).ptr();
  if (token_details->service() != GaiaConstants::kGaiaOAuth2LoginRefreshToken)
    return;

  auth_token_.clear();
  if (type == chrome::NOTIFICATION_TOKEN_AVAILABLE) {
    TokenService* service = TokenServiceFactory::GetForProfile(profile_);
    refresh_token_ = service->GetOAuth2LoginRefreshToken();
  } else {
    refresh_token_.clear();
  }
  FOR_EACH_OBSERVER(Observer, observers_, OnOAuth2RefreshTokenChanged());
}

//=============================== DocumentsService =============================

DocumentsService::DocumentsService()
    : profile_(NULL),
      gdata_auth_service_(new GDataAuthService()),
      operation_registry_(new GDataOperationRegistry),
      weak_ptr_factory_(this),
      // The weak pointers is used to post tasks to UI thread.
      weak_ptr_bound_to_ui_thread_(weak_ptr_factory_.GetWeakPtr()) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

DocumentsService::~DocumentsService() {
  gdata_auth_service_->RemoveObserver(this);
}

void DocumentsService::Initialize(Profile* profile) {
  profile_ = profile;
  // AddObserver() should be called before Initialize() as it could change
  // the refresh token.
  gdata_auth_service_->AddObserver(this);
  gdata_auth_service_->Initialize(profile);
}

void DocumentsService::CancelAll() {
  operation_registry_->CancelAll();
}

void DocumentsService::Authenticate(const AuthStatusCallback& callback) {
  gdata_auth_service_->StartAuthentication(operation_registry_.get(),
                                           callback);
}

void DocumentsService::GetDocuments(const GURL& url,
                                    const GetDataCallback& callback) {
  GetDocumentsOperation* operation =
      new GetDocumentsOperation(operation_registry_.get(), profile_, callback);
  if (!url.is_empty())
    operation->SetUrl(url);
  StartOperationOnUIThread(operation);
}

void DocumentsService::GetAccountMetadata(const GetDataCallback& callback) {
  GetAccountMetadataOperation* operation =
      new GetAccountMetadataOperation(operation_registry_.get(),
                                      profile_,
                                      callback);
  StartOperationOnUIThread(operation);
}

void DocumentsService::DownloadDocument(
    const FilePath& virtual_path,
    const GURL& document_url,
    DocumentExportFormat format,
    const DownloadActionCallback& callback) {
  DownloadFile(
      virtual_path,
      chrome_browser_net::AppendQueryParameter(document_url,
                                               "exportFormat",
                                               GetExportFormatParam(format)),
      callback);
}

void DocumentsService::DownloadFile(const FilePath& virtual_path,
                                    const GURL& document_url,
                                    const DownloadActionCallback& callback) {
  StartOperationOnUIThread(
      new DownloadFileOperation(operation_registry_.get(), profile_, callback,
                                document_url));
}

void DocumentsService::DeleteDocument(const GURL& document_url,
                                      const EntryActionCallback& callback) {
  StartOperationOnUIThread(
      new DeleteDocumentOperation(operation_registry_.get(), profile_, callback,
                                  document_url));
}

void DocumentsService::CreateDirectory(
    const GURL& parent_content_url,
    const FilePath::StringType& directory_name,
    const GetDataCallback& callback) {
  StartOperationOnUIThread(
      new CreateDirectoryOperation(operation_registry_.get(), profile_,
                                   callback, parent_content_url,
                                   directory_name));
}

void DocumentsService::CopyDocument(const GURL& document_url,
                                    const FilePath::StringType& new_name,
                                    const GetDataCallback& callback) {
  StartOperationOnUIThread(
      new CopyDocumentOperation(operation_registry_.get(), profile_, callback,
                                document_url, new_name));
}

void DocumentsService::RenameResource(const GURL& resource_url,
                                      const FilePath::StringType& new_name,
                                      const EntryActionCallback& callback) {
  StartOperationOnUIThread(
      new RenameResourceOperation(operation_registry_.get(), profile_, callback,
                                  resource_url, new_name));
}

void DocumentsService::AddResourceToDirectory(
    const GURL& parent_content_url,
    const GURL& resource_url,
    const EntryActionCallback& callback) {
  StartOperationOnUIThread(
      new AddResourceToDirectoryOperation(operation_registry_.get(),
                                          profile_,
                                          callback,
                                          parent_content_url,
                                          resource_url));
}

void DocumentsService::RemoveResourceFromDirectory(
    const GURL& parent_content_url,
    const GURL& resource_url,
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  StartOperationOnUIThread(
      new RemoveResourceFromDirectoryOperation(operation_registry_.get(),
                                               profile_,
                                               callback,
                                               parent_content_url,
                                               resource_url,
                                               resource_id));
}

void DocumentsService::InitiateUpload(const InitiateUploadParams& params,
                                      const InitiateUploadCallback& callback) {
  if (params.resumable_create_media_link.is_empty()) {
    if (!callback.is_null()) {
      callback.Run(HTTP_BAD_REQUEST, GURL());
    }
    return;
  }

  StartOperationOnUIThread(
      new InitiateUploadOperation(operation_registry_.get(), profile_, callback,
                                  params));
}

void DocumentsService::ResumeUpload(const ResumeUploadParams& params,
                                    const ResumeUploadCallback& callback) {
  StartOperationOnUIThread(
      new ResumeUploadOperation(operation_registry_.get(), profile_, callback,
                                params));
}

void DocumentsService::OnOAuth2RefreshTokenChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void DocumentsService::StartOperationOnUIThread(
    GDataOperationInterface* operation) {
  operation->SetReAuthenticateCallback(
      base::Bind(&DocumentsService::RetryOperation,
                 weak_ptr_factory_.GetWeakPtr()));
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&DocumentsService::StartOperation,
                 weak_ptr_bound_to_ui_thread_,
                 operation));  // |operation| is self-contained
}

void DocumentsService::StartOperation(GDataOperationInterface* operation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!gdata_auth_service_->IsFullyAuthenticated()) {
    // Fetch OAuth2 authentication token from the refresh token first.
    gdata_auth_service_->StartAuthentication(
        operation_registry_.get(),
        base::Bind(&DocumentsService::OnOperationAuthRefresh,
                   weak_ptr_factory_.GetWeakPtr(),
                   operation));
    return;
  }

  operation->Start(gdata_auth_service_->oauth2_auth_token());
}

void DocumentsService::OnOperationAuthRefresh(
    GDataOperationInterface* operation,
    GDataErrorCode code,
    const std::string& auth_token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (code == HTTP_SUCCESS) {
    DCHECK(gdata_auth_service_->IsPartiallyAuthenticated());
    StartOperation(operation);
  } else {
    operation->OnAuthFailed(code);
  }
}

void DocumentsService::RetryOperation(GDataOperationInterface* operation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  gdata_auth_service_->ClearOAuth2Token();
  // User authentication might have expired - rerun the request to force
  // auth token refresh.
  StartOperation(operation);
}

}  // namespace gdata
