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

namespace media_router {

// A button representing a sink in the Cast dialog. It is highlighted when
// hovered.
class CastDialogSinkButton : public HoverButton {
 public:
  // Icon sizes in DIP. These values are also used by the "no devices" view for
  // consistency.
  static int kPrimaryIconSize;
  static int kSecondaryIconSize;

  CastDialogSinkButton(views::ButtonListener* button_listener,
                       const UIMediaSink& sink);
  ~CastDialogSinkButton() override;

  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnEnabledChanged() override;

  const UIMediaSink& sink() const { return sink_; }

 private:
  UIMediaSink sink_;

  DISALLOW_COPY_AND_ASSIGN(CastDialogSinkButton);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_SINK_BUTTON_H_
