// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MESSAGE_CENTER_MESSAGE_CENTER_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_MESSAGE_CENTER_MESSAGE_CENTER_FRAME_VIEW_H_

#include "ui/views/window/non_client_view.h"

namespace views {
class Label;
class LabelButton;
class BubbleBorder;
}

namespace message_center {

// The non-client frame view of the message center widget.
class MessageCenterFrameView : public views::NonClientFrameView {
 public:
  explicit MessageCenterFrameView();
  virtual ~MessageCenterFrameView();

  // NonClientFrameView overrides:
  virtual gfx::Rect GetBoundsForClientView() const override;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  virtual int NonClientHitTest(const gfx::Point& point) override;
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask) override;
  virtual void ResetWindowControls() override;
  virtual void UpdateWindowIcon() override;
  virtual void UpdateWindowTitle() override;
  virtual void SizeConstraintsChanged() override;

  // View overrides:
  virtual gfx::Insets GetInsets() const override;
  virtual const char* GetClassName() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MessageCenterFrameView);
};

}  // namespace message_center

#endif  // CHROME_BROWSER_UI_VIEWS_MESSAGE_CENTER_MESSAGE_CENTER_FRAME_VIEW_H_
