// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cookie_modal_dialog.h"
#import "chrome/browser/cocoa/cookie_prompt_window_controller.h"

#import <Cocoa/Cocoa.h>

#include "app/l10n_util_mac.h"
#import "base/cocoa_protocols_mac.h"
#include "base/mac_util.h"
#include "base/scoped_nsobject.h"
#include "base/logging.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"

void CookiePromptModalDialog::CreateAndShowDialog() {
  scoped_nsobject<CookiePromptWindowController> controller(
      [[CookiePromptWindowController alloc] initWithDialog:this]);
  [controller.get() doModalDialog:this];

  // Other than JavaScriptAppModalDialog, the cross-platform part of this class
  // does not call |CompleteDialog()|, an explicit call is required.
  CompleteDialog();
  Cleanup();
  delete this;
}

// The functions below are used by the automation framework.

void CookiePromptModalDialog::AcceptWindow() {
  NOTIMPLEMENTED();
}

void CookiePromptModalDialog::CancelWindow() {
  NOTIMPLEMENTED();
}

void CookiePromptModalDialog::CloseModalDialog() {
  dialog_ = nil;
}

// This is only used by the app-modal dialog machinery on windows.
NativeDialog CookiePromptModalDialog::CreateNativeDialog() {
  NOTIMPLEMENTED();
  return nil;
}
