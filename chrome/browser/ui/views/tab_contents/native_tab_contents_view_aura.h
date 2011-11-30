// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_NATIVE_TAB_CONTENTS_VIEW_AURA_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_NATIVE_TAB_CONTENTS_VIEW_AURA_H_
#pragma once

#include "chrome/browser/ui/views/tab_contents/native_tab_contents_view.h"
#include "ui/base/events.h"
#include "ui/views/widget/native_widget_aura.h"

namespace aura {
class DropTargetEvent;
}

namespace ui {
class OSExchangeDataProviderAura;
}

class TabContents;

class NativeTabContentsViewAura : public views::NativeWidgetAura,
                                  public NativeTabContentsView {
 public:
  explicit NativeTabContentsViewAura(
      internal::NativeTabContentsViewDelegate* delegate);
  virtual ~NativeTabContentsViewAura();

  TabContents* GetTabContents() const;

 private:
  // Overridden from NativeTabContentsView:
  virtual void InitNativeTabContentsView() OVERRIDE;
  virtual void Unparent() OVERRIDE;
  virtual RenderWidgetHostView* CreateRenderWidgetHostView(
      RenderWidgetHost* render_widget_host) OVERRIDE;
  virtual gfx::NativeWindow GetTopLevelNativeWindow() const OVERRIDE;
  virtual void SetPageTitle(const string16& title) OVERRIDE;
  virtual void StartDragging(const WebDropData& drop_data,
                             WebKit::WebDragOperationsMask ops,
                             const SkBitmap& image,
                             const gfx::Point& image_offset) OVERRIDE;
  virtual void CancelDrag() OVERRIDE;
  virtual bool IsDoingDrag() const OVERRIDE;
  virtual void SetDragCursor(WebKit::WebDragOperation operation) OVERRIDE;
  virtual views::NativeWidget* AsNativeWidget() OVERRIDE;

  // Overridden from views::NativeWidgetAura:
  virtual void OnBoundsChanged(const gfx::Rect& old_bounds,
                               const gfx::Rect& new_bounds) OVERRIDE;
  virtual bool OnMouseEvent(aura::MouseEvent* event) OVERRIDE;
  virtual void OnDragEntered(const aura::DropTargetEvent& event) OVERRIDE;
  virtual int OnDragUpdated(const aura::DropTargetEvent& event) OVERRIDE;
  virtual void OnDragExited() OVERRIDE;
  virtual int OnPerformDrop(const aura::DropTargetEvent& event) OVERRIDE;

  // Informs the renderer that the drag operation it initiated has ended and
  // |ops| drag operations were applied.
  void EndDrag(WebKit::WebDragOperationsMask ops);

  internal::NativeTabContentsViewDelegate* delegate_;

  WebKit::WebDragOperationsMask current_drag_op_;

  DISALLOW_COPY_AND_ASSIGN(NativeTabContentsViewAura);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_NATIVE_TAB_CONTENTS_VIEW_AURA_H_
