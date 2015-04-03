// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_sheet.h"
#import "chrome/browser/ui/cocoa/single_web_contents_dialog_manager_cocoa.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"

namespace web_modal {

SingleWebContentsDialogManager*
WebContentsModalDialogManager::CreateNativeWebModalManager(
    gfx::NativeWindow dialog,
    web_modal::SingleWebContentsDialogManagerDelegate* delegate) {
  base::scoped_nsobject<CustomConstrainedWindowSheet> sheet(
      [[CustomConstrainedWindowSheet alloc] initWithCustomWindow:dialog]);
  return new SingleWebContentsDialogManagerCocoa(nullptr, sheet, delegate);
}

}  // namespace web_modal
