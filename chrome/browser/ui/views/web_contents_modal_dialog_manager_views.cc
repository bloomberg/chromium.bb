// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_modal/web_contents_modal_dialog_manager.h"

#include "chrome/browser/ui/views/native_web_contents_modal_dialog_manager_views.h"
#include "components/web_modal/single_web_contents_dialog_manager.h"

namespace web_modal {

SingleWebContentsDialogManager*
WebContentsModalDialogManager::CreateNativeWebModalManager(
    gfx::NativeWindow dialog,
    SingleWebContentsDialogManagerDelegate* native_delegate) {
  return new NativeWebContentsModalDialogManagerViews(dialog, native_delegate);
}

}  // namespace web_modal
