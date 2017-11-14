// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MESSAGE_CENTER_MESSAGE_CENTER_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_MESSAGE_CENTER_MESSAGE_CENTER_FRAME_VIEW_H_

#include "base/macros.h"
#include "ui/views/window/non_client_view.h"

namespace message_center {

// The non-client frame view of the message center widget.
class MessageCenterFrameView : public views::NonClientFrameView {
 public:
  explicit MessageCenterFrameView();
  ~MessageCenterFrameView() override;

  // NonClientFrameView overrides:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask) override;
  void ResetWindowControls() override;
  void UpdateWindowIcon() override;
  void UpdateWindowTitle() override;
  void SizeConstraintsChanged() override;

  // View overrides:
  gfx::Insets GetInsets() const override;
  const char* GetClassName() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MessageCenterFrameView);
};

}  // namespace message_center

#endif  // CHROME_BROWSER_UI_VIEWS_MESSAGE_CENTER_MESSAGE_CENTER_FRAME_VIEW_H_
