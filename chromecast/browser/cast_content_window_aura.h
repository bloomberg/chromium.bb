// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_AURA_H_
#define CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_AURA_H_

#include "base/macros.h"
#include "chromecast/browser/cast_content_window.h"

namespace content {
class WebContents;
}  // namespace content

namespace chromecast {
namespace shell {

class TouchBlocker;

class CastContentWindowAura : public CastContentWindow {
 public:
  ~CastContentWindowAura() override;

  // CastContentWindow implementation.
  void CreateWindowForWebContents(
      content::WebContents* web_contents,
      CastWindowManager* window_manager,
      bool is_visible,
      VisibilityPriority visibility_priority) override;
  void RequestVisibility(VisibilityPriority visibility_priority) override;
  void RequestMoveOut() override;

  void EnableTouchInput(bool enabled) override;

 private:
  friend class CastContentWindow;

  // This class should only be instantiated by CastContentWindow::Create.
  CastContentWindowAura(bool is_touch_enabled);

  const bool is_touch_enabled_;
  std::unique_ptr<TouchBlocker> touch_blocker_;

  DISALLOW_COPY_AND_ASSIGN(CastContentWindowAura);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_AURA_H_
