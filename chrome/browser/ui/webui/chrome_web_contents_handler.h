// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROME_WEB_CONTENTS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROME_WEB_CONTENTS_HANDLER_H_

#include "base/compiler_specific.h"
#include "ui/web_dialogs/web_dialog_web_contents_delegate.h"

class ChromeWebContentsHandler
    : public ui::WebDialogWebContentsDelegate::WebContentsHandler {
 public:
  ChromeWebContentsHandler();
  virtual ~ChromeWebContentsHandler();

  // Overridden from WebDialogWebContentsDelegate::WebContentsHandler:
  virtual content::WebContents* OpenURLFromTab(
      content::BrowserContext* context,
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE;
  virtual void AddNewContents(content::BrowserContext* context,
                              content::WebContents* source,
                              content::WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeWebContentsHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROME_WEB_CONTENTS_HANDLER_H_
