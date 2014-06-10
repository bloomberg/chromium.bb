// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MESSAGE_CENTER_MESSAGE_CENTER_WIDGET_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_MESSAGE_CENTER_MESSAGE_CENTER_WIDGET_DELEGATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_tray.h"
#include "ui/message_center/message_center_tray_delegate.h"
#include "ui/message_center/views/message_center_view.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

namespace message_center {

enum Alignment {
  ALIGNMENT_TOP = 1 << 0,
  ALIGNMENT_LEFT = 1 << 1,
  ALIGNMENT_BOTTOM = 1 << 2,
  ALIGNMENT_RIGHT = 1 << 3,
  ALIGNMENT_NONE = 1 << 4,
};

struct PositionInfo {
  int max_height;

  // Alignment of the message center relative to the center of the screen.
  Alignment message_center_alignment;

  // Alignment of the taskbar relative to the center of the screen.
  Alignment taskbar_alignment;

  // Point relative to which message center is positioned.
  gfx::Point inital_anchor_point;
};

class WebNotificationTray;
class MessageCenterFrameView;

// MessageCenterWidgetDelegate is the message center's client view. It also
// creates the message center widget and sets the notifications.
//
////////////////////////////////////////////////////////////////////////////////
class MessageCenterWidgetDelegate : public views::WidgetDelegate,
                                    public message_center::MessageCenterView,
                                    public views::WidgetObserver {
 public:
  MessageCenterWidgetDelegate(WebNotificationTray* tray,
                              MessageCenterTray* mc_tray,
                              bool initially_settings_visible,
                              const PositionInfo& pos_info,
                              const base::string16& title);
  virtual ~MessageCenterWidgetDelegate();

  // WidgetDelegate overrides:
  virtual View* GetContentsView() OVERRIDE;
  virtual views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;

  // WidgetObserver overrides:
  virtual void OnWidgetActivationChanged(views::Widget* widget,
                                         bool active) OVERRIDE;
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE;

  // View overrides:
  virtual void PreferredSizeChanged() OVERRIDE;
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual gfx::Size GetMaximumSize() const OVERRIDE;
  virtual int GetHeightForWidth(int width) const OVERRIDE;
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;

 private:
  // Creates and initializes the message center widget.
  void InitWidget();

  // Shifts the message center anchor point such that the mouse click point is
  // along the middle 60% of the width of the message center if taskbar is
  // horizontal aligned. If vertically aligned, ensures that mouse click point
  // is along the height of the message center (at least at a corner).
  gfx::Point GetCorrectedAnchor(gfx::Size calculated_size);

  // Calculates the message center bounds using the position info and the
  // corrected anchor.
  gfx::Rect GetMessageCenterBounds();

  // Insets of the message center border (set in MessageCenterFrameView).
  gfx::Insets border_insets_;

  // Info necessary to calculate the estimated position of the message center.
  PositionInfo pos_info_;

  WebNotificationTray* tray_;
};

}  // namespace message_center

#endif  // CHROME_BROWSER_UI_VIEWS_MESSAGE_CENTER_MESSAGE_CENTER_WIDGET_DELEGATE_H_
