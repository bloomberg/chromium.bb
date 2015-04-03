// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/single_web_contents_dialog_manager_cocoa.h"

#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_sheet.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_mac.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_sheet_controller.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"

SingleWebContentsDialogManagerCocoa::SingleWebContentsDialogManagerCocoa(
    ConstrainedWindowMac* client,
    id<ConstrainedWindowSheet> sheet,
    web_modal::SingleWebContentsDialogManagerDelegate* delegate)
    : client_(client),
      sheet_([sheet retain]),
      delegate_(delegate),
      shown_(false) {
  if (client)
    client->set_manager(this);
}

SingleWebContentsDialogManagerCocoa::~SingleWebContentsDialogManagerCocoa() {
}

void SingleWebContentsDialogManagerCocoa::Show() {
  if (shown_)
    return;

  content::WebContents* web_contents = delegate_->GetWebContents();
  NSWindow* parent_window = web_contents->GetTopLevelNativeWindow();
  TabDialogs* tab_dialogs = TabDialogs::FromWebContents(web_contents);
  // |tab_dialogs| is null when |web_contents| is inside a packaged app window.
  NSView* parent_view = tab_dialogs ? tab_dialogs->GetDialogParentView()
                                    : web_contents->GetNativeView();
  if (!parent_window || !parent_view)
    return;

  shown_ = true;
  [[ConstrainedWindowSheetController controllerForParentWindow:parent_window]
      showSheet:sheet_ forParentView:parent_view];
}

void SingleWebContentsDialogManagerCocoa::Hide() {
}

void SingleWebContentsDialogManagerCocoa::Close() {
  [[ConstrainedWindowSheetController controllerForSheet:sheet_]
      closeSheet:sheet_];
  if (client_) {
    client_->set_manager(nullptr);
    client_->OnDialogClosing();  // |client_| might delete itself here.
    client_ = nullptr;
  }
  delegate_->WillClose(dialog());
}

void SingleWebContentsDialogManagerCocoa::Focus() {
}

void SingleWebContentsDialogManagerCocoa::Pulse() {
  [[ConstrainedWindowSheetController controllerForSheet:sheet_]
      pulseSheet:sheet_];
}

void SingleWebContentsDialogManagerCocoa::HostChanged(
    web_modal::WebContentsModalDialogHost* new_host) {
}

gfx::NativeWindow SingleWebContentsDialogManagerCocoa::dialog() {
  return [sheet_ sheetWindow];
}
