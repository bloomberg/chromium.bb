// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_ATTACHMENTS_ATTACHMENT_UPLOADER_IMPL_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_ATTACHMENTS_ATTACHMENT_UPLOADER_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <unordered_map>

#include "base/macros.h"
#include "base/threading/non_thread_safe.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine/attachments/attachment_uploader.h"
#include "google_apis/gaia/oauth2_token_service_request.h"
#include "net/url_request/url_request_context_getter.h"

class GURL;

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace syncer {

// An implementation of AttachmentUploader.
class AttachmentUploaderImpl : public AttachmentUploader,
                               public base::NonThreadSafe {
 public:
  // |sync_service_url| is the URL of the sync service.
  //
  // |url_request_context_getter| provides a URLRequestContext.
  //
  // |account_id| is the account id to use for uploads.
  //
  // |scopes| is the set of scopes to use for uploads.
  //
  // |token_service_provider| provides an OAuth2 token service.
  //
  // |store_birthday| is the raw, sync store birthday.
  //
  // |model_type| is the model type this uploader is used with.
  AttachmentUploaderImpl(
      const GURL& sync_service_url,
      const scoped_refptr<net::URLRequestContextGetter>&
          url_request_context_getter,
      const std::string& account_id,
      const OAuth2TokenService::ScopeSet& scopes,
      const scoped_refptr<OAuth2TokenServiceRequest::TokenServiceProvider>&
          token_service_provider,
      const std::string& store_birthday,
      ModelType model_type);
  ~AttachmentUploaderImpl() override;

  // AttachmentUploader implementation.
  void UploadAttachment(const Attachment& attachment,
                        const UploadCallback& callback) override;

  // Return the URL for the given |sync_service_url| and |attachment_id|.
  static GURL GetURLForAttachmentId(const GURL& sync_service_url,
                                    const AttachmentId& attachment_id);

  // Format crc32c to pass into X-Goog-Hash header.
  //
  // The value is base64 encoded, big-endian format.  Suitable for use in the
  // X-Goog-Hash header
  // (https://cloud.google.com/storage/docs/reference-headers#xgooghash).
  static std::string FormatCrc32cHash(uint32_t crc32c);

  // Apply common settings to |fetcher|, suitable for both uploads and
  // downloads.
  static void ConfigureURLFetcherCommon(
      net::URLFetcher* fetcher,
      const std::string& auth_token,
      const std::string& raw_store_birthday,
      ModelType model_type,
      net::URLRequestContextGetter* request_context_getter);

 private:
  class UploadState;
  using UniqueId = std::string;
  using StateMap = std::unordered_map<UniqueId, std::unique_ptr<UploadState>>;

  void OnUploadStateStopped(const UniqueId& unique_id);

  GURL sync_service_url_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
  std::string account_id_;
  OAuth2TokenService::ScopeSet scopes_;
  scoped_refptr<OAuth2TokenServiceRequest::TokenServiceProvider>
      token_service_provider_;
  std::string raw_store_birthday_;
  StateMap state_map_;
  ModelType model_type_;

  // Must be last data member.
  base::WeakPtrFactory<AttachmentUploaderImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AttachmentUploaderImpl);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_ATTACHMENTS_ATTACHMENT_UPLOADER_IMPL_H_
