// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_H_
#define CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace aura {
class WindowTreeHost;
}

namespace content {
class BrowserContext;
class WebContents;
}

namespace gfx {
class Size;
}

namespace chromecast {

class CastContentWindow {
 public:
  CastContentWindow();

  // Removes the window from the screen.
  ~CastContentWindow();

  // Create a window with the given size.
  scoped_ptr<content::WebContents> Create(
      const gfx::Size& initial_size,
      content::BrowserContext* browser_context);

 private:
#if defined(USE_AURA)
  scoped_ptr<aura::WindowTreeHost> window_tree_host_;
#endif

  DISALLOW_COPY_AND_ASSIGN(CastContentWindow);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_H_
