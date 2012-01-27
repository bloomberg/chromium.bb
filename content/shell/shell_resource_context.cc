// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_resource_context.h"

#include "content/browser/chrome_blob_storage_context.h"
#include "content/shell/shell_url_request_context_getter.h"

namespace content {

ShellResourceContext::ShellResourceContext(
    ShellURLRequestContextGetter* getter,
    ChromeBlobStorageContext* blob_storage_context)
    : getter_(getter),
      blob_storage_context_(blob_storage_context) {
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
}

}  // namespace content
