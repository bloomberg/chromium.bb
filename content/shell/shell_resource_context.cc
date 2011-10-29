// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_resource_context.h"

#include "content/browser/chrome_blob_storage_context.h"
#include "content/browser/download/download_id_factory.h"
#include "content/shell/shell_url_request_context_getter.h"

namespace content {

ShellResourceContext::ShellResourceContext(
    ShellURLRequestContextGetter* getter,
    ChromeBlobStorageContext* blob_storage_context,
    DownloadIdFactory* download_id_factory)
    : getter_(getter),
      blob_storage_context_(blob_storage_context),
      download_id_factory_(download_id_factory) {
}

ShellResourceContext::~ShellResourceContext() {
}

void ShellResourceContext::EnsureInitialized() const {
  const_cast<ShellResourceContext*>(this)->InitializeInternal();
}

void ShellResourceContext::InitializeInternal() {
  set_request_context(getter_->GetURLRequestContext());
  set_host_resolver(getter_->host_resolver());
  set_blob_storage_context(blob_storage_context_);
  set_download_id_factory(download_id_factory_);
}

}  // namespace content
