// Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/in_process_webkit/dom_storage_permission_request.h"

DOMStoragePermissionRequest::DOMStoragePermissionRequest(
    const std::string& host,
    bool file_exists,
    int64 size,
    const base::Time last_modified)
    : host_(host),
      file_exists_(file_exists),
      size_(size),
      last_modified_(last_modified),
      event_(true, false) {  // manual reset, not initially signaled
}

ContentSetting DOMStoragePermissionRequest::WaitOnResponse() {
  event_.Wait();
  return response_content_setting_;
}

void DOMStoragePermissionRequest::SendResponse(ContentSetting content_setting) {
  response_content_setting_ = content_setting;
  event_.Signal();
}

// static
void DOMStoragePermissionRequest::DoSomething(
    DOMStoragePermissionRequest *dom_storage_permission_request) {
  // TODO(jorlow/darin): This function is just a placeholder until we work out
  //                     exactly what needs to happen here.
  dom_storage_permission_request->SendResponse(CONTENT_SETTING_BLOCK);
}
