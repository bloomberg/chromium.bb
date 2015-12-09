// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_H_
#define CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_contents_observer.h"

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
namespace shell {

class CastContentWindow : public content::WebContentsObserver {
 public:
  CastContentWindow();

  // Removes the window from the screen.
  ~CastContentWindow() override;

  // Sets the window's background to be transparent (call before
  // CreateWindowTree).
  void SetTransparent() { transparent_ = true; }

  // Create a window with the given size for |web_contents|.
  void CreateWindowTree(const gfx::Size& initial_size,
                        content::WebContents* web_contents);

  scoped_ptr<content::WebContents> CreateWebContents(
      const gfx::Size& initial_size,
      content::BrowserContext* browser_context);

  // content::WebContentsObserver implementation:
  void DidFirstVisuallyNonEmptyPaint() override;
  void MediaStoppedPlaying(const MediaPlayerId& id) override;
  void MediaStartedPlaying(const MediaPlayerId& id) override;
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;

 private:
#if defined(USE_AURA)
  scoped_ptr<aura::WindowTreeHost> window_tree_host_;
#endif
  bool transparent_;

  DISALLOW_COPY_AND_ASSIGN(CastContentWindow);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_H_
