// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/chooser_dialog_cocoa.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/chrome_extension_chooser_dialog.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_sheet.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_window.h"
#import "chrome/browser/ui/cocoa/extensions/chooser_dialog_cocoa_controller.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"

ChooserDialogCocoa::ChooserDialogCocoa(content::WebContents* web_contents,
                                       ChooserController* chooser_controller)
    : web_contents_(web_contents), chooser_controller_(chooser_controller) {
  DCHECK(web_contents_);
  DCHECK(chooser_controller_);
  chooser_controller_->set_observer(this);
  chooser_dialog_cocoa_controller_.reset([[ChooserDialogCocoaController alloc]
      initWithChooserDialogCocoa:this
               chooserController:chooser_controller_]);
}

ChooserDialogCocoa::~ChooserDialogCocoa() {}

void ChooserDialogCocoa::OnConstrainedWindowClosed(
    ConstrainedWindowMac* window) {
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void ChooserDialogCocoa::OnOptionsInitialized() {
  if (chooser_dialog_cocoa_controller_)
    [chooser_dialog_cocoa_controller_ onOptionsInitialized];
}

void ChooserDialogCocoa::OnOptionAdded(size_t index) {
  if (chooser_dialog_cocoa_controller_)
    [chooser_dialog_cocoa_controller_ onOptionAdded:index];
}

void ChooserDialogCocoa::OnOptionRemoved(size_t index) {
  if (chooser_dialog_cocoa_controller_)
    [chooser_dialog_cocoa_controller_ onOptionRemoved:index];
}

void ChooserDialogCocoa::ShowDialog() {
  base::scoped_nsobject<NSWindow> window([[ConstrainedWindowCustomWindow alloc]
      initWithContentRect:[[chooser_dialog_cocoa_controller_ view] bounds]]);
  [[window contentView] addSubview:[chooser_dialog_cocoa_controller_ view]];
  base::scoped_nsobject<CustomConstrainedWindowSheet> sheet(
      [[CustomConstrainedWindowSheet alloc] initWithCustomWindow:window]);
  constrained_window_ =
      CreateAndShowWebModalDialogMac(this, web_contents_, sheet);
}

void ChooserDialogCocoa::Dismissed() {
  if (constrained_window_)
    constrained_window_->CloseWebContentsModalDialog();
}

void ChromeExtensionChooserDialog::ShowDialog(
    ChooserController* chooser_controller) const {
  web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents());
  if (manager) {
    // These objects will delete themselves when the dialog closes.
    ChooserDialogCocoa* chooser_dialog =
        new ChooserDialogCocoa(web_contents(), chooser_controller);
    chooser_dialog->ShowDialog();
  }
}
