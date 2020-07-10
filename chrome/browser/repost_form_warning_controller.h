// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REPOST_FORM_WARNING_CONTROLLER_H_
#define CHROME_BROWSER_REPOST_FORM_WARNING_CONTROLLER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "content/public/browser/web_contents_observer.h"

// This class is used to continue or cancel a pending reload when the
// repost form warning is shown. It is owned by the platform-specific
// |TabModalConfirmDialog{Gtk, Mac, Views, WebUI}| classes.
class RepostFormWarningController : public TabModalConfirmDialogDelegate,
                                    public content::WebContentsObserver {
 public:
  explicit RepostFormWarningController(content::WebContents* web_contents);
  ~RepostFormWarningController() override;

 private:
  // TabModalConfirmDialogDelegate methods:
  base::string16 GetTitle() override;
  base::string16 GetDialogMessage() override;
  base::string16 GetAcceptButtonTitle() override;
  void OnAccepted() override;
  void OnCanceled() override;
  void OnClosed() override;

  // content::WebContentsObserver methods:
  void BeforeFormRepostWarningShow() override;

  DISALLOW_COPY_AND_ASSIGN(RepostFormWarningController);
};

#endif  // CHROME_BROWSER_REPOST_FORM_WARNING_CONTROLLER_H_
