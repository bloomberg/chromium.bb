// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/guest_view/extension_view/extension_view_internal_api.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/stop_find_action.h"
#include "extensions/common/api/extension_view_internal.h"

namespace extensionview = extensions::core_api::extension_view_internal;

namespace extensions {

bool ExtensionViewInternalExtensionFunction::RunAsync() {
  int instance_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &instance_id));
  ExtensionViewGuest* guest = ExtensionViewGuest::From(
      render_view_host()->GetProcess()->GetID(), instance_id);
  if (!guest)
    return false;
  return RunAsyncSafe(guest);
}

bool ExtensionViewInternalNavigateFunction::RunAsyncSafe(
    ExtensionViewGuest* guest) {
  scoped_ptr<extensionview::Navigate::Params> params(
      extensionview::Navigate::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  std::string src = params->src;
  guest->NavigateGuest(src, true /* force_navigation */);
  return true;
}

}  // namespace extensions

