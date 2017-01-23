// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_LINUX_H_
#define CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_LINUX_H_

#include <memory>

#include "base/macros.h"
#include "chromecast/browser/cast_content_window.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace content {
class BrowserContext;
class WebContents;
}

namespace chromecast {
namespace shell {

class CastContentWindowLinux : public CastContentWindow,
                               public content::WebContentsObserver {
 public:
  // Removes the window from the screen.
  ~CastContentWindowLinux() override;

  // CastContentWindow implementation.
  void SetTransparent() override;
  void ShowWebContents(content::WebContents* web_contents,
                       CastWindowManager* window_manager) override;
  std::unique_ptr<content::WebContents> CreateWebContents(
      content::BrowserContext* browser_context) override;

  // content::WebContentsObserver implementation:
  void DidFirstVisuallyNonEmptyPaint() override;
  void MediaStartedPlaying(const MediaPlayerInfo& media_info,
                           const MediaPlayerId& id) override;
  void MediaStoppedPlaying(const MediaPlayerInfo& media_info,
                           const MediaPlayerId& id) override;
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;

 private:
  friend class CastContentWindow;

  // This class should only be instantiated by CastContentWindow::Create.
  CastContentWindowLinux();

  bool transparent_;

  DISALLOW_COPY_AND_ASSIGN(CastContentWindowLinux);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_LINUX_H_
