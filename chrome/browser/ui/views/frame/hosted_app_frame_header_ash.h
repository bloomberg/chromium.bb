// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_HOSTED_APP_FRAME_HEADER_ASH_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_HOSTED_APP_FRAME_HEADER_ASH_H_

#include "ash/frame/default_frame_header.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"

namespace gfx {
class Canvas;
class RenderText;
}  // namespace gfx

namespace extensions {
class HostedAppBrowserController;
}  // namespace extensions

// Helper class for managing the hosted app window header.
class HostedAppFrameHeaderAsh : public ash::DefaultFrameHeader {
 public:
  HostedAppFrameHeaderAsh(
      extensions::HostedAppBrowserController* app_controller,
      views::Widget* frame,
      views::View* header_view,
      ash::FrameCaptionButtonContainerView* caption_button_container);
  ~HostedAppFrameHeaderAsh() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(HostedAppNonClientFrameViewAshTest, HostedAppFrame);

  // Create a render text for this header.
  std::unique_ptr<gfx::RenderText> CreateRenderText();

  // Refresh the text inside the render texts.
  void UpdateRenderTexts();

  // Render the app and domain to the right of the window title, truncating the
  // the window title first if the combination doesn't fit.
  void LayoutRenderTexts(const gfx::Rect& available_title_bounds,
                         int title_width,
                         int app_and_domain_width);

  // ash::DefaultFrameHeader:
  void PaintTitleBar(gfx::Canvas* canvas) override;
  void LayoutHeader() override;

  extensions::HostedAppBrowserController* app_controller_;  // Weak.

  // The render text for the window title.
  std::unique_ptr<gfx::RenderText> title_render_text_;

  // The render text for the app and domain in the title bar.
  std::unique_ptr<gfx::RenderText> app_and_domain_render_text_;

  // The name of the app.
  const base::string16 app_name_;

  // The app and domain string to display in the title bar.
  const base::string16 app_and_domain_;

  DISALLOW_COPY_AND_ASSIGN(HostedAppFrameHeaderAsh);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_HOSTED_APP_FRAME_HEADER_ASH_H_
