// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_WEB_CONTENTS_H_
#define HEADLESS_PUBLIC_WEB_CONTENTS_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "headless/public/headless_export.h"
#include "url/gurl.h"

class SkBitmap;

namespace content {
class WebContents;
}

namespace headless {
class WebFrame;

// Class representing contents of a browser tab.
// Should be accessed from browser main thread.
class HEADLESS_EXPORT WebContents {
 public:
  virtual ~WebContents() {}

  class Observer {
   public:
    // Will be called on browser thread.
    virtual void DidNavigateMainFrame() = 0;
    virtual void DocumentOnLoadCompletedInMainFrame() = 0;

    virtual void OnLoadProgressChanged(double progress) = 0;
    virtual void AddMessageToConsole(const std::string& message) = 0;
    virtual bool ShouldSuppressDialogs(WebContents* source) = 0;
    virtual void OnModalAlertDialog(const std::string& message) = 0;
    virtual bool OnModalConfirmDialog(const std::string& message) = 0;
    virtual std::string OnModalPromptDialog(
        const std::string& message,
        const std::string& default_value) = 0;

   protected:
    explicit Observer(WebContents* web_contents);
    virtual ~Observer();

   private:
    class ObserverImpl;
    scoped_ptr<ObserverImpl> observer_;

    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  class Settings {
    virtual void SetWebSecurityEnabled(bool enabled) = 0;
    virtual void SetLocalStorageEnabled(bool enabled) = 0;
    virtual void SetJavaScriptCanOpenWindowsAutomatically(bool enabled) = 0;
    virtual void SetAllowScriptsToCloseWindows(bool enabled) = 0;
    virtual void SetImagesEnabled(bool enabled) = 0;
    virtual void SetJavascriptEnabled(bool enabled) = 0;
    virtual void SetXSSAuditorEnabled(bool enabled) = 0;
    virtual void SetDefaultTextEncoding(const std::string& encoding) = 0;
  };

  virtual Settings* GetSettings() = 0;
  virtual const Settings* GetSettings() const = 0;

  virtual NavigationController* GetController() = 0;
  virtual const NavigationController* GetController() const = 0;

  virtual std::string GetTitle() const = 0;
  virtual const GURL& GetVisibleURL() const = 0;
  virtual const GURL& GetLastCommittedURL() const = 0;
  virtual bool IsLoading() const = 0;

  virtual bool SetViewportSize(const gfx::Size& size) const = 0;

  // Returns main frame for web page. Note that the returned interface should
  // only be used on the renderer main thread.
  virtual WebFrame* GetMainFrame() = 0;

  // Requests browser tab to navigate to given url.
  virtual void OpenURL(const GURL& url) = 0;

  using ScreenshotCallback = base::Callback<void(scoped_ptr<SkBitmap>)>;
  // Requests an image of web contents.
  virtual void GetScreenshot(const ScreenshotCallback& callback) = 0;

 protected:
  virtual content::WebContents* web_contents() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebContents);
};

}  // namespace headless

#endif  // HEADLESS_PUBLIC_WEB_CONTENTS_H_
