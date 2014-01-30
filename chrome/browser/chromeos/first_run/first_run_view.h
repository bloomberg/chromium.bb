// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FIRST_RUN_FIRST_RUN_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_FIRST_RUN_FIRST_RUN_VIEW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/views/view.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace views {
class WebView;
}

namespace chromeos {

class FirstRunActor;

// WebUI view used for first run tutorial.
class FirstRunView : public views::View,
                     public content::WebContentsDelegate {
 public:
  FirstRunView();
  void Init(content::BrowserContext* context);
  FirstRunActor* GetActor();

  // Overriden from views::View.
  virtual void Layout() OVERRIDE;
  virtual void RequestFocus() OVERRIDE;

  content::WebContents* GetWebContents();

 private:
  // Overriden from content::WebContentsDelegate.
  virtual bool HandleContextMenu(
      const content::ContextMenuParams& params) OVERRIDE;
  virtual bool PreHandleGestureEvent(
      content::WebContents* source,
      const blink::WebGestureEvent& event) OVERRIDE;

  views::WebView* web_view_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FIRST_RUN_FIRST_RUN_VIEW_H_

