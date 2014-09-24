// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LINK_DISAMBIGUATION_LINK_DISAMBIGUATION_POPUP_H_
#define CHROME_BROWSER_UI_VIEWS_LINK_DISAMBIGUATION_LINK_DISAMBIGUATION_POPUP_H_

#include "content/public/browser/web_contents_view_delegate.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}

namespace ui {
class MouseEvent;
}

// Creates a popup with a zoomed bitmap rendered by Blink of an area in web
// |content| that received an ambiguous Gesture event. This allows the user to
// select which of the links their first Gesture event overlapped. The popup
// generates a new Gesture event which is sent back to the provided |callback|.
class LinkDisambiguationPopup {
 public:
  LinkDisambiguationPopup();
  ~LinkDisambiguationPopup();

  // Creates and shows the Popup. |zoomed_bitmap| is the scaled-up image of the
  // ambiguous web content. |target_rect| is the original, unzoomed rectangle
  // in DIPs in the coordinate system of |content|. We will convert received
  // gestures in the popup to the coordinate system of |content| and as an
  // offset within |target_rect|, and then call the |callback| with the
  // transformed coordinates GestureEvent.
  void Show(const SkBitmap& zoomed_bitmap,
            const gfx::Rect& target_rect,
            const gfx::NativeView content,
            const base::Callback<void(ui::GestureEvent*)>& gesture_cb,
            const base::Callback<void(ui::MouseEvent*)>& mouse_cb);
  void Close();

 private:
  class ZoomBubbleView;

  // It is possible that |view_| can be destroyed by its widget instead of
  // closed explicitly by us. In that case we need to be notified that it has
  // been destroyed so we can invalidate our own pointer to that view.
  void InvalidateBubbleView();

  // A non-owning pointer to the calling window that contains the unzoomed web
  // content bitmap, that we will be sending the GestureEvents received back to.
  const aura::Window* content_;
  ZoomBubbleView* view_;

  DISALLOW_COPY_AND_ASSIGN(LinkDisambiguationPopup);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LINK_DISAMBIGUATION_LINK_DISAMBIGUATION_POPUP_H_
