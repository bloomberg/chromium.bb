// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FRAME_PANEL_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_FRAME_PANEL_CONTROLLER_H_
#pragma once

#include <gtk/gtk.h>

#include "third_party/cros/chromeos_wm_ipc_enums.h"
#include "ui/base/x/x11_util.h"
#include "views/controls/button/button.h"

class BrowserView;
class SkBitmap;
typedef unsigned long XID;

namespace views {
class ImageButton;
class ImageView;
class Label;
class MouseEvent;
class WidgetGtk;
}

namespace chromeos {

// Controls interactions with the WM for popups / panels.
class PanelController {
 public:
  enum State {
    INITIAL,
    EXPANDED,
    MINIMIZED,
  };

  // Delegate to control panel's appearance and behavior.
  class Delegate {
   public:
    // Retrieves the title string of the panel.
    virtual string16 GetPanelTitle() = 0;

    // Retrieves the icon to use in the panel's titlebar.
    virtual SkBitmap GetPanelIcon() = 0;

    // Can the panel be closed?  Called before ClosePanel() when the close
    // button is pressed to give beforeunload handlers a chance to cancel.
    virtual bool CanClosePanel() = 0;

    // Close the panel. Called when a close button is pressed.
    virtual void ClosePanel() = 0;
  };

  PanelController(Delegate* delegate_window,
                  GtkWindow* window);
  virtual ~PanelController() {}

  // Initializes the panel controller with the initial state of the focus and
  // the window bounds.
  void Init(bool initial_focus, const gfx::Rect& init_bounds, XID creator_xid,
            WmIpcPanelUserResizeType resize_type);

  bool TitleMousePressed(const views::MouseEvent& event);
  void TitleMouseReleased(const views::MouseEvent& event, bool canceled);
  bool TitleMouseDragged(const views::MouseEvent& event);
  bool PanelClientEvent(GdkEventClient* event);
  void OnFocusIn();
  void OnFocusOut();

  void UpdateTitleBar();
  void Close();

  void SetState(State state);

 private:
  class TitleContentView : public views::View,
                           public views::ButtonListener {
   public:
    explicit TitleContentView(PanelController* panelController);
    virtual ~TitleContentView();
    virtual void Layout();
    virtual bool OnMousePressed(const views::MouseEvent& event);
    virtual void OnMouseReleased(const views::MouseEvent& event, bool canceled);
    virtual bool OnMouseDragged(const views::MouseEvent& event);
    void OnFocusIn();
    void OnFocusOut();
    void OnClose();

    views::ImageView* title_icon() { return title_icon_; }
    views::Label* title_label() { return title_label_; }
    views::ImageButton* close_button() { return close_button_; }

    // ButtonListener methods.
    virtual void ButtonPressed(views::Button* sender,
                               const views::Event& event);
   private:
    views::ImageView* title_icon_;
    views::Label* title_label_;
    views::ImageButton* close_button_;
    PanelController* panel_controller_;
    DISALLOW_COPY_AND_ASSIGN(TitleContentView);
  };

  // Called from TitleContentView's ButtonPressed handler.
  void OnCloseButtonPressed();

  // Dispatches client events to PanelController instances
  static bool OnPanelClientEvent(
      GtkWidget* widget,
      GdkEventClient* event,
      PanelController* panel_controller);

  // Panel's delegate.
  Delegate* delegate_;

  // Gtk object for content.
  GtkWindow* panel_;
  // X id for content.
  XID panel_xid_;

  // Views object representing title.
  views::WidgetGtk* title_window_;
  // Gtk object representing title.
  GtkWidget* title_;
  // X id representing title.
  XID title_xid_;

  // Views object, holds title and close button.
  TitleContentView* title_content_;

  // Is the panel expanded or collapsed?
  bool expanded_;

  // Is the mouse button currently down?
  bool mouse_down_;

  // Cursor's absolute position when the mouse button was pressed.
  int mouse_down_abs_x_;
  int mouse_down_abs_y_;

  // Cursor's offset from the upper-left corner of the titlebar when the
  // mouse button was pressed.
  int mouse_down_offset_x_;
  int mouse_down_offset_y_;

  // Is the titlebar currently being dragged?  That is, has the cursor
  // moved more than kDragThreshold away from its starting position?
  bool dragging_;

  // GTK client event handler id.
  int client_event_handler_id_;

  DISALLOW_COPY_AND_ASSIGN(PanelController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FRAME_PANEL_CONTROLLER_H_
