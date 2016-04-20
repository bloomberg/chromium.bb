// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_HEADLESS_WEB_CONTENTS_H_
#define HEADLESS_PUBLIC_HEADLESS_WEB_CONTENTS_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "headless/public/headless_export.h"
#include "url/gurl.h"

namespace headless {
class HeadlessDevToolsTarget;

// Class representing contents of a browser tab. Should be accessed from browser
// main thread.
class HEADLESS_EXPORT HeadlessWebContents {
 public:
  virtual ~HeadlessWebContents() {}

  // TODO(skyostil): Replace this with an equivalent client API.
  class Observer {
   public:
    // All the following notifications will be called on browser main thread.
    virtual void DocumentOnLoadCompletedInMainFrame() {}
    virtual void DidFinishNavigation(bool success) {}

    // Indicates that this HeadlessWebContents instance is now ready to be
    // inspected using a HeadlessDevToolsClient.
    virtual void DevToolsTargetReady() {}

   protected:
    Observer() {}
    virtual ~Observer() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  // Add or remove an observer to receive events from this WebContents.
  // |observer| must outlive this class or be removed prior to being destroyed.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Return a DevTools target corresponding to this tab. Note that this method
  // won't return a valid value until Observer::DevToolsTargetReady has been
  // signaled.
  virtual HeadlessDevToolsTarget* GetDevToolsTarget() = 0;

 private:
  friend class HeadlessWebContentsImpl;
  HeadlessWebContents() {}

  DISALLOW_COPY_AND_ASSIGN(HeadlessWebContents);
};

}  // namespace headless

#endif  // HEADLESS_PUBLIC_HEADLESS_WEB_CONTENTS_H_
