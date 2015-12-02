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
    virtual void DocumentOnLoadCompletedInMainFrame() = 0;

    // TODO(altimin): More OnSomething() methods will go here.

   protected:
    explicit Observer(WebContents* web_contents);
    virtual ~Observer();

   private:
    class ObserverImpl;
    scoped_ptr<ObserverImpl> observer_;

    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  // Returns main frame for web page.
  // Should be called on renderer main thread.
  virtual WebFrame* MainFrame() = 0;

  // Requests browser tab to nagivate to given url.
  // Should be called on browser main thread.
  virtual void OpenURL(const GURL& url) = 0;

  using ScreenshotCallback = base::Callback<void(scoped_ptr<SkBitmap>)>;
  // Requests an image of web contents.
  // Should be called on browser main thread.
  virtual void GetScreenshot(const ScreenshotCallback& callback) = 0;

 protected:
  virtual content::WebContents* web_contents() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebContents);
};

}  // namespace headless

#endif  // HEADLESS_PUBLIC_WEB_CONTENTS_H_
