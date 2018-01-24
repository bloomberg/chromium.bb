// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_WEB_VIEW_H_
#define CHROMECAST_BROWSER_CAST_WEB_VIEW_H_

#include "base/time/time.h"
#include "chromecast/browser/cast_content_window.h"
#include "content/public/browser/web_contents.h"

namespace chromecast {

class CastWindowManager;

// A simplified interface for loading and displaying WebContents in cast_shell.
class CastWebView {
 public:
  class Delegate : public shell::CastContentWindow::Delegate {
   public:
    // Called when the page has stopped. ie: A 404 occured when loading the page
    // or if the render process crashes. |error_code| will return a net::Error
    // describing the failure, or net::OK if the page closed naturally.
    virtual void OnPageStopped(int error_code) = 0;

    // Called during WebContentsDelegate::LoadingStateChanged.
    // |loading| indicates if web_contents_ IsLoading or not.
    virtual void OnLoadingStateChanged(bool loading) = 0;
  };

  virtual ~CastWebView() {}

  virtual shell::CastContentWindow* window() const = 0;

  virtual content::WebContents* web_contents() const = 0;

  // Navigates to |url|. The loaded page will be preloaded if MakeVisible has
  // not been called on the object.
  virtual void LoadUrl(GURL url) = 0;

  // Begins the close process for this page (ie. triggering document.onunload).
  // A consumer of the class can be notified when the process has been finished
  // via Delegate::OnPageStopped(). The page will be torn down after
  // |shutdown_delay| has elapsed, or sooner if required.
  virtual void ClosePage(const base::TimeDelta& shutdown_delay) = 0;

  // Makes the page visible to the user.
  virtual void Show(CastWindowManager* window_manager) = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_WEB_VIEW_H_
