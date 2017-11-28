// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FIRST_RUN_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_FIRST_RUN_BUBBLE_H_

#include <memory>

#include "base/macros.h"
#include "ui/events/event.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/controls/link_listener.h"

namespace views {
class EventMonitor;
class View;
}

class Browser;

class FirstRunBubble : public views::BubbleDialogDelegateView,
                       public views::LinkListener {
 public:
  // If |anchor_view| is not null, the bubble is anchored to it; otherwise, it
  // is anchored to the rect returned by
  // bubble_anchor_util::GetPageInfoAnchorRect().
  FirstRunBubble(Browser* browser,
                 views::View* anchor_view,
                 gfx::NativeWindow parent_window);
  ~FirstRunBubble() override;

  // Usual uses of this dialog don't actually need the created FirstRunBubble,
  // but unit-tests do, so the ShowForTest() version returns it.
  static void Show(Browser* browser);
  static FirstRunBubble* ShowForTest(views::View* anchor_view);

 protected:
  // views::BubbleDialogDelegateView overrides:
  void Init() override;
  int GetDialogButtons() const override;

 private:
  // This class observes keyboard events, mouse clicks and touch down events
  // targeted towards the anchor widget and dismisses the first run bubble
  // accordingly.
  class FirstRunBubbleCloser : public ui::EventHandler {
   public:
    FirstRunBubbleCloser(FirstRunBubble* bubble, gfx::NativeWindow parent);
    ~FirstRunBubbleCloser() override;

    // ui::EventHandler overrides.
    void OnKeyEvent(ui::KeyEvent* event) override;
    void OnMouseEvent(ui::MouseEvent* event) override;
    void OnGestureEvent(ui::GestureEvent* event) override;

   private:
    void CloseBubble();

    // The bubble instance.
    FirstRunBubble* bubble_;

    std::unique_ptr<views::EventMonitor> event_monitor_;

    DISALLOW_COPY_AND_ASSIGN(FirstRunBubbleCloser);
  };

  // views::LinkListener overrides:
  void LinkClicked(views::Link* source, int event_flags) override;

  Browser* const browser_;
  FirstRunBubbleCloser bubble_closer_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunBubble);
};

namespace first_run {}  // namespace first_run

#endif  // CHROME_BROWSER_UI_VIEWS_FIRST_RUN_BUBBLE_H_
