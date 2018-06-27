// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_SINK_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_SINK_BUTTON_H_

#include "chrome/browser/ui/media_router/ui_media_sink.h"
#include "chrome/browser/ui/views/hover_button.h"

namespace ui {
class MouseEvent;
}

namespace views {
class InkDrop;
}

namespace media_router {

// A button representing a sink in the Cast dialog. It is highlighted when
// hovered or selected.
class CastDialogSinkButton : public HoverButton {
 public:
  // Icon sizes in DIP. These values are also used by the "no devices" view for
  // consistency.
  static int kPrimaryIconSize;
  static int kSecondaryIconSize;

  CastDialogSinkButton(views::ButtonListener* button_listener,
                       const UIMediaSink& sink);
  ~CastDialogSinkButton() override;

  // Sets |is_selected_| and updates the button's ink drop accordingly.
  void SetSelected(bool is_selected);

  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;

  // Returns the text that should be shown on the main action button of the Cast
  // dialog when this button is selected.
  base::string16 GetActionText() const;

  // Changes the ink drop to the pressed state without animation.
  void SnapInkDropToActivated();

  const UIMediaSink& sink() const { return sink_; }

 private:
  UIMediaSink sink_;

  bool is_selected_ = false;

  DISALLOW_COPY_AND_ASSIGN(CastDialogSinkButton);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_SINK_BUTTON_H_
