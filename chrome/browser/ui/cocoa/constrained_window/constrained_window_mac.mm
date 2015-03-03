// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_mac.h"

#include "base/memory/scoped_ptr.h"
#include "base/logging.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_sheet.h"
#import "chrome/browser/ui/cocoa/single_web_contents_dialog_manager_cocoa.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/guest_view/guest_view_base.h"

using web_modal::WebContentsModalDialogManager;

ConstrainedWindowMac::ConstrainedWindowMac(
    ConstrainedWindowMacDelegate* delegate,
    content::WebContents* web_contents,
    id<ConstrainedWindowSheet> sheet)
    : delegate_(delegate) {
  DCHECK(sheet);
  extensions::GuestViewBase* guest_view =
      extensions::GuestViewBase::FromWebContents(web_contents);
  // For embedded WebContents, use the embedder's WebContents for constrained
  // window.
  web_contents = guest_view && guest_view->embedder_web_contents() ?
                    guest_view->embedder_web_contents() : web_contents;

  auto manager = WebContentsModalDialogManager::FromWebContents(web_contents);
  scoped_ptr<SingleWebContentsDialogManagerCocoa> native_manager(
      new SingleWebContentsDialogManagerCocoa(this, sheet, manager));
  manager->ShowDialogWithManager([sheet sheetWindow], native_manager.Pass());
}

ConstrainedWindowMac::~ConstrainedWindowMac() {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!manager_);
}

void ConstrainedWindowMac::CloseWebContentsModalDialog() {
  if (manager_)
    manager_->Close();
}

void ConstrainedWindowMac::OnDialogClosing() {
  if (delegate_)
    delegate_->OnConstrainedWindowClosed(this);
}
