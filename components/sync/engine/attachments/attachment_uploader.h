// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_ATTACHMENTS_ATTACHMENT_UPLOADER_H_
#define COMPONENTS_SYNC_ENGINE_ATTACHMENTS_ATTACHMENT_UPLOADER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/attachments/attachment.h"
#include "google_apis/gaia/oauth2_token_service_request.h"

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace syncer {

// AttachmentUploader is responsible for uploading attachments to the server.
class AttachmentUploader {
 public:
  // The result of an UploadAttachment operation.
  enum UploadResult {
    UPLOAD_SUCCESS,            // No error, attachment was uploaded
                               // successfully.
    UPLOAD_TRANSIENT_ERROR,    // A transient error occurred, try again later.
    UPLOAD_UNSPECIFIED_ERROR,  // An unspecified error occurred.
  };

  using UploadCallback =
      base::Callback<void(const UploadResult&, const AttachmentId&)>;

  virtual ~AttachmentUploader();

  // Upload |attachment| and invoke |callback| when done.
  //
  // |callback| will be invoked when the operation has completed (successfully
  // or otherwise).
  //
  // |callback| will receive an UploadResult code and the AttachmentId of the
  // newly uploaded attachment.
  virtual void UploadAttachment(const Attachment& attachment,
                                const UploadCallback& callback) = 0;

  // Create an instance of AttachmentUploader.
  //
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
  static std::unique_ptr<AttachmentUploader> Create(
      const GURL& sync_service_url,
      const scoped_refptr<net::URLRequestContextGetter>&
          url_request_context_getter,
      const std::string& account_id,
      const OAuth2TokenService::ScopeSet scopes,
      const scoped_refptr<OAuth2TokenServiceRequest::TokenServiceProvider>&
          token_service_provider,
      const std::string& store_birthday,
      ModelType model_type);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_ATTACHMENTS_ATTACHMENT_UPLOADER_H_
