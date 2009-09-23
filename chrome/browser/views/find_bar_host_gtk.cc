// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/find_bar_host.h"

#include <gdk/gdkkeysyms.h>

#include "chrome/browser/find_bar_controller.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "views/widget/widget_gtk.h"

class FindBarHostWidget : public views::WidgetGtk {
 public:
  explicit FindBarHostWidget(FindBarHost* find_bar)
      : WidgetGtk(TYPE_CHILD),
        find_bar_(find_bar) {
    // Don't let WidgetGtk manage our lifetime. We want our lifetime to
    // coincide with TabContents.
    set_delete_on_destroy(false);
  }

  void OnDestroy(GtkWidget* widget) {
    find_bar_->OnFinalMessage();
  }

 private:
  FindBarHost* find_bar_;

  DISALLOW_COPY_AND_ASSIGN(FindBarHostWidget);
};

void FindBarHost::UpdateWindowEdges(const gfx::Rect& new_pos) {
  // TODO(davemoore) move the windows implementation to CustomFrameWindow so we
  // don't have to implement it for gtk
  NOTIMPLEMENTED();
}

void FindBarHost::AudibleAlert() {
  // TODO(davemoore) implement
  NOTIMPLEMENTED();
}

views::Widget* FindBarHost::CreateHost() {
  return new FindBarHostWidget(this);
}

void FindBarHost::SetDialogPositionNative(const gfx::Rect& new_pos,
                                          bool no_redraw) {
  host_->SetBounds(new_pos);
  host_->Show();
}

void FindBarHost::GetDialogPositionNative(gfx::Rect* avoid_overlapping_rect) {
  // TODO(davemoore) implement
  NOTIMPLEMENTED();
}


gfx::NativeView FindBarHost::GetNativeView(BrowserView* browser_view) {
  return static_cast<views::WidgetGtk*>(
      browser_view->GetWidget())->window_contents();
}


NativeWebKeyboardEvent FindBarHost::GetKeyboardEvent(
     const TabContents* contents,
     const views::Textfield::Keystroke& key_stroke) {
  return NativeWebKeyboardEvent(key_stroke.event());
}

bool FindBarHost::ShouldForwardKeystrokeToWebpageNative(
  const views::Textfield::Keystroke& key_stroke) {
    return true;
}

