// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/attachments/attachment_uploader.h"

#include "base/memory/ptr_util.h"
#include "components/sync/engine_impl/attachments/attachment_uploader_impl.h"

namespace syncer {

AttachmentUploader::~AttachmentUploader() {}

std::unique_ptr<AttachmentUploader> AttachmentUploader::Create(
    const GURL& sync_service_url,
    const scoped_refptr<net::URLRequestContextGetter>&
        url_request_context_getter,
    const std::string& account_id,
    const OAuth2TokenService::ScopeSet scopes,
    const scoped_refptr<OAuth2TokenServiceRequest::TokenServiceProvider>&
        token_service_provider,
    const std::string& store_birthday,
    ModelType model_type) {
  return base::MakeUnique<AttachmentUploaderImpl>(
      sync_service_url, url_request_context_getter, account_id, scopes,
      token_service_provider, store_birthday, model_type);
}

}  // namespace syncer
