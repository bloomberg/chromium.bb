// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/app_view/app_view_internal_api.h"

#include "extensions/browser/guest_view/app_view/app_view_guest.h"
#include "extensions/common/api/app_view_internal.h"


namespace extensions {

namespace appview = core_api::app_view_internal;

AppViewInternalAttachFrameFunction::
    AppViewInternalAttachFrameFunction() {
}

bool AppViewInternalAttachFrameFunction::RunAsync() {
  scoped_ptr<appview::AttachFrame::Params> params(
      appview::AttachFrame::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GURL url = extension()->GetResourceURL(params->url);
  EXTENSION_FUNCTION_VALIDATE(url.is_valid());

  return AppViewGuest::CompletePendingRequest(browser_context(),
                                              url,
                                              params->guest_instance_id,
                                              extension_id());
}

AppViewInternalDenyRequestFunction::
    AppViewInternalDenyRequestFunction() {
}

bool AppViewInternalDenyRequestFunction::RunAsync() {
  scoped_ptr<appview::DenyRequest::Params> params(
      appview::DenyRequest::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Since the URL passed into AppViewGuest:::CompletePendingRequest is invalid,
  // a new <appview> WebContents will not be created.
  return AppViewGuest::CompletePendingRequest(browser_context(),
                                              GURL(),
                                              params->guest_instance_id,
                                              extension_id());
}

}  // namespace extensions
