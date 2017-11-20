// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_FOCUS_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_FOCUS_HELPER_H_

#include "ui/gfx/native_widget_types.h"
#include "ui/views/view_tracker.h"

namespace content {
class WebContents;
}

namespace views {
class FocusManager;
class Widget;
}

// A chrome specific helper class that handles focus management.
class ChromeWebContentsViewFocusHelper {
 public:
  explicit ChromeWebContentsViewFocusHelper(content::WebContents* web_contents);
  ~ChromeWebContentsViewFocusHelper();

  void StoreFocus();
  bool RestoreFocus();
  void ResetStoredFocus();
  bool Focus();
  bool TakeFocus(bool reverse);

 private:
  gfx::NativeView GetActiveNativeView();
  views::Widget* GetTopLevelWidget();
  views::FocusManager* GetFocusManager();

  // Used to store the last focused view.
  views::ViewTracker last_focused_view_tracker_;

  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(ChromeWebContentsViewFocusHelper);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_CHROME_WEB_CONTENTS_VIEW_FOCUS_HELPER_H_
