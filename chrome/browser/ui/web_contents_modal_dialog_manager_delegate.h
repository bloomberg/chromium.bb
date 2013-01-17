// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_CONTENTS_MODAL_DIALOG_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_UI_WEB_CONTENTS_MODAL_DIALOG_MANAGER_DELEGATE_H_

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

  // Fills in |point| with the coordinates for positioning web contents modal
  // dialogs. The dialog should position itself such that its top center is at
  // |point|. Return false if the default position should be used.
  // TODO(wittman): Remove this function once we have migrated positioning
  // functionality to a ShowDialog function on WebContentsModalDialogManager.
  virtual bool GetDialogTopCenter(gfx::Point* point);

 protected:
  virtual ~WebContentsModalDialogManagerDelegate();
};

#endif  // CHROME_BROWSER_UI_WEB_CONTENTS_MODAL_DIALOG_MANAGER_DELEGATE_H_
