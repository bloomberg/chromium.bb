// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLOCKED_CONTENT_APP_MODAL_DIALOG_HELPER_H_
#define CHROME_BROWSER_UI_BLOCKED_CONTENT_APP_MODAL_DIALOG_HELPER_H_

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"

// A helper for app modal dialogs that blocks creation of pop-unders.
class AppModalDialogHelper : public content::WebContentsObserver {
 public:
  explicit AppModalDialogHelper(content::WebContents* dialog_host);
  ~AppModalDialogHelper() override;

 private:
  // Overridden from WebContentsObserver:
  void WebContentsDestroyed() override;

  content::WebContents* popup_;

  DISALLOW_COPY_AND_ASSIGN(AppModalDialogHelper);
};

#endif  // CHROME_BROWSER_UI_BLOCKED_CONTENT_APP_MODAL_DIALOG_HELPER_H_
