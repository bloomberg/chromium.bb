// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCREENSAVER_SCREENSAVER_VIEW_H_
#define ASH_SCREENSAVER_SCREENSAVER_VIEW_H_
#pragma once

#include "ash/ash_export.h"
#include "base/callback.h"
#include "content/public/browser/web_contents_observer.h"
#include "googleurl/src/gurl.h"
#include "ui/views/widget/widget_delegate.h"

namespace content {
class BrowserContent;
}

namespace views {
class WebView;
}

namespace ash {

namespace test {
class ScreensaverViewTest;
}

ASH_EXPORT void ShowScreensaver(const GURL& url);
ASH_EXPORT void CloseScreensaver();

typedef
    base::Callback<views::WebView*(content::BrowserContext*)> WebViewFactory;

namespace internal {

// Shows a URL as a screensaver. The screensaver window is fullscreen,
// always on top of every other window and will reload the URL if the
// renderer crashes for any reason.
class ScreensaverView : public views::WidgetDelegateView,
                        public content::WebContentsObserver {
 public:
  static void ShowScreensaver(const GURL& url);
  static void CloseScreensaver();

 private:
  friend class test::ScreensaverViewTest;

  explicit ScreensaverView(const GURL& url);
  virtual ~ScreensaverView();

  // views::WidgetDelegate overrides.
  virtual views::View* GetContentsView() OVERRIDE;

  // content::WebContentsObserver overrides.
  virtual void RenderViewGone(base::TerminationStatus status) OVERRIDE;

  void Show();
  void Close();

  // Creates and adds web contents to our view.
  void AddChildWebContents();
  // Load the screensaver in the WebView's webcontent. If the webcontents
  // don't exist, they'll be created by WebView.
  void LoadScreensaver();
  // Creates and shows a frameless full screen window containing our view.
  void ShowWindow();

  // For testing purposes.
  static ASH_EXPORT ScreensaverView* GetInstance();

  // URL to show in the screensaver.
  GURL url_;

  // Host for the extension that implements this dialog.
  views::WebView* screensaver_webview_;

  // Window that holds the screensaver webview.
  views::Widget* container_window_;

  DISALLOW_COPY_AND_ASSIGN(ScreensaverView);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SCREENSAVER_SCREENSAVER_VIEW_H_
