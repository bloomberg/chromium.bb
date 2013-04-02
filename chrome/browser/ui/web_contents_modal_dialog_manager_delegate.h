// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_CONTENTS_MODAL_DIALOG_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_UI_WEB_CONTENTS_MODAL_DIALOG_MANAGER_DELEGATE_H_

class WebContentsModalDialogHost;

namespace content {
class WebContents;
}

namespace gfx {
class Point;
}

class WebContentsModalDialogManagerDelegate {
 public:
  // Changes the blocked state of |web_contents|. WebContentses are considered
  // blocked while displaying a web contents modal dialog. During that time
  // renderer host will ignore any UI interaction within WebContents outside of
  // the currently displaying dialog.
  virtual void SetWebContentsBlocked(content::WebContents* web_contents,
                                     bool blocked);

  // Returns the WebContentsModalDialogHost for use in positioning web contents
  // modal dialogs within the browser window.
  virtual WebContentsModalDialogHost* GetWebContentsModalDialogHost();

 protected:
  virtual ~WebContentsModalDialogManagerDelegate();
};

#endif  // CHROME_BROWSER_UI_WEB_CONTENTS_MODAL_DIALOG_MANAGER_DELEGATE_H_
