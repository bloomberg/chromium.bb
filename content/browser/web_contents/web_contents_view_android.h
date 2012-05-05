// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_ANDROID_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_ANDROID_H_
#pragma once

#include "content/public/browser/web_contents_view.h"

class WebContentsViewAndroid : public content::WebContentsView {
 public:
  explicit WebContentsViewAndroid(content::WebContents* web_contents);
  virtual ~WebContentsViewAndroid();

  // WebContentsView implementation --------------------------------------------

  virtual void CreateView(const gfx::Size& initial_size) OVERRIDE;
  virtual content::RenderWidgetHostView* CreateViewForWidget(
      content::RenderWidgetHost* render_widget_host) OVERRIDE;

  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeView GetContentNativeView() const OVERRIDE;
  virtual gfx::NativeWindow GetTopLevelNativeWindow() const OVERRIDE;
  virtual void GetContainerBounds(gfx::Rect* out) const OVERRIDE;
  virtual void SetPageTitle(const string16& title) OVERRIDE;
  virtual void OnTabCrashed(base::TerminationStatus status,
                            int error_code) OVERRIDE;
  virtual void SizeContents(const gfx::Size& size) OVERRIDE;
  virtual void RenderViewCreated(content::RenderViewHost* host) OVERRIDE;
  virtual void Focus() OVERRIDE;
  virtual void SetInitialFocus() OVERRIDE;
  virtual void StoreFocus() OVERRIDE;
  virtual void RestoreFocus() OVERRIDE;
  virtual bool IsDoingDrag() const OVERRIDE;
  virtual void CancelDragAndCloseTab() OVERRIDE;
  virtual WebDropData* GetDropData() const OVERRIDE;
  virtual bool IsEventTracking() const OVERRIDE;
  virtual void CloseTabAfterEventTracking() OVERRIDE;
  virtual void GetViewBounds(gfx::Rect* out) const OVERRIDE;

  // Backend implementation of RenderViewHostDelegate::View.
  virtual void CreateNewWindow(
      int route_id,
      const ViewHostMsg_CreateWindow_Params& params) OVERRIDE;
  virtual void CreateNewWidget(int route_id,
                               WebKit::WebPopupType popup_type) OVERRIDE;
  virtual void CreateNewFullscreenWidget(int route_id) OVERRIDE;
  virtual void ShowCreatedWindow(int route_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture) OVERRIDE;
  virtual void ShowCreatedWidget(int route_id,
                                 const gfx::Rect& initial_pos) OVERRIDE;
  virtual void ShowCreatedFullscreenWidget(int route_id) OVERRIDE;
  virtual void ShowContextMenu(
      const content::ContextMenuParams& params) OVERRIDE;
  virtual void ShowPopupMenu(const gfx::Rect& bounds,
                             int item_height,
                             double item_font_size,
                             int selected_item,
                             const std::vector<WebMenuItem>& items,
                             bool right_aligned) OVERRIDE;
  virtual void StartDragging(const WebDropData& drop_data,
                             WebKit::WebDragOperationsMask allowed_ops,
                             const SkBitmap& image,
                             const gfx::Point& image_offset) OVERRIDE;
  virtual void UpdateDragCursor(WebKit::WebDragOperation operation) OVERRIDE;
  virtual void GotFocus() OVERRIDE;
  virtual void TakeFocus(bool reverse) OVERRIDE;

 private:
  // The WebContents whose contents we display.
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsViewAndroid);
};

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_ANDROID_H_
