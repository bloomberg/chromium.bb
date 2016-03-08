// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_mac.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_sheet.h"
#import "chrome/browser/ui/cocoa/single_web_contents_dialog_manager_cocoa.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_thread.h"

using web_modal::WebContentsModalDialogManager;

ConstrainedWindowMac::ConstrainedWindowMac(
    ConstrainedWindowMacDelegate* delegate,
    content::WebContents* web_contents,
    id<ConstrainedWindowSheet> sheet)
    : delegate_(delegate) {
  DCHECK(sheet);

  // |web_contents| may be embedded within a chain of nested GuestViews. If it
  // is, follow the chain of embedders to the outermost WebContents and use it.
  web_contents =
      guest_view::GuestViewBase::GetTopLevelWebContents(web_contents);

  auto manager = WebContentsModalDialogManager::FromWebContents(web_contents);
  scoped_ptr<SingleWebContentsDialogManagerCocoa> native_manager(
      new SingleWebContentsDialogManagerCocoa(this, sheet, manager));
  manager->ShowDialogWithManager([sheet sheetWindow],
                                 std::move(native_manager));
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
