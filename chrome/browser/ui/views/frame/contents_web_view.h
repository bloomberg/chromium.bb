// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_WEB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_WEB_VIEW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/controls/webview/webview.h"

class StatusBubbleViews;

// ContentsWebView is used to present WebContents of active tab.
class ContentsWebView : public views::WebView {
 public:
  explicit ContentsWebView(content::BrowserContext* browser_context);
  virtual ~ContentsWebView();

  // Sets the status bubble, which should be repositioned every time
  // this view changes visible bounds.
  void SetStatusBubble(StatusBubbleViews* status_bubble);

  // views::View overrides:
  virtual bool NeedsNotificationWhenVisibleBoundsChange() const OVERRIDE;
  virtual void OnVisibleBoundsChanged() OVERRIDE;
  virtual void ViewHierarchyChanged(const ViewHierarchyChangedDetails& details)
      OVERRIDE;
  virtual void OnThemeChanged() OVERRIDE;

 private:
  StatusBubbleViews* status_bubble_;

  DISALLOW_COPY_AND_ASSIGN(ContentsWebView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_WEB_VIEW_H_
