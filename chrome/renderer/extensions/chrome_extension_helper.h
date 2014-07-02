// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_CHROME_EXTENSION_HELPER_H_
#define CHROME_RENDERER_EXTENSIONS_CHROME_EXTENSION_HELPER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_observer_tracker.h"

struct WebApplicationInfo;

namespace extensions {

// RenderView plumbing for Chrome-specific extension features.
// See also extensions/renderer/extension_helper.h.
class ChromeExtensionHelper
    : public content::RenderViewObserver,
      public content::RenderViewObserverTracker<ChromeExtensionHelper> {
 public:
  explicit ChromeExtensionHelper(content::RenderView* render_view);
  virtual ~ChromeExtensionHelper();

 private:
  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // IPC message handlers.
  void OnGetApplicationInfo();

  // The app info that we are processing. This is used when installing an app
  // via application definition. The in-progress web app is stored here while
  // its manifest and icons are downloaded.
  scoped_ptr<WebApplicationInfo> pending_app_info_;

  DISALLOW_COPY_AND_ASSIGN(ChromeExtensionHelper);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_CHROME_EXTENSION_HELPER_H_
