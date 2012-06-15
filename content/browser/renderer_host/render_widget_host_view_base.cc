// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_base.h"

#include "base/logging.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

#if defined(TOOLKIT_GTK)
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "content/browser/renderer_host/gtk_window_utils.h"
#endif

namespace content {

// static
RenderWidgetHostViewPort* RenderWidgetHostViewPort::FromRWHV(
    RenderWidgetHostView* rwhv) {
  return static_cast<RenderWidgetHostViewPort*>(rwhv);
}

// static
RenderWidgetHostViewPort* RenderWidgetHostViewPort::CreateViewForWidget(
    content::RenderWidgetHost* widget) {
  return FromRWHV(RenderWidgetHostView::CreateViewForWidget(widget));
}

RenderWidgetHostViewBase::RenderWidgetHostViewBase()
    : popup_type_(WebKit::WebPopupTypeNone),
      mouse_locked_(false),
      showing_context_menu_(false),
      selection_text_offset_(0),
      selection_range_(ui::Range::InvalidRange()) {
}

RenderWidgetHostViewBase::~RenderWidgetHostViewBase() {
  DCHECK(!mouse_locked_);
}

void RenderWidgetHostViewBase::SetBackground(const SkBitmap& background) {
  background_ = background;
}

const SkBitmap& RenderWidgetHostViewBase::GetBackground() {
  return background_;
}

void RenderWidgetHostViewBase::SelectionChanged(const string16& text,
                                                size_t offset,
                                                const ui::Range& range) {
  selection_text_ = text;
  selection_text_offset_ = offset;
  selection_range_.set_start(range.start());
  selection_range_.set_end(range.end());
}

bool RenderWidgetHostViewBase::IsShowingContextMenu() const {
  return showing_context_menu_;
}

void RenderWidgetHostViewBase::SetShowingContextMenu(bool showing) {
  DCHECK_NE(showing_context_menu_, showing);
  showing_context_menu_ = showing;
}

bool RenderWidgetHostViewBase::IsMouseLocked() {
  return mouse_locked_;
}

void RenderWidgetHostViewBase::UnhandledWheelEvent(
    const WebKit::WebMouseWheelEvent& event) {
  // Most implementations don't need to do anything here.
}

void RenderWidgetHostViewBase::SetPopupType(WebKit::WebPopupType popup_type) {
  popup_type_ = popup_type;
}

WebKit::WebPopupType RenderWidgetHostViewBase::GetPopupType() {
  return popup_type_;
}

BrowserAccessibilityManager*
    RenderWidgetHostViewBase::GetBrowserAccessibilityManager() const {
  return browser_accessibility_manager_.get();
}

void RenderWidgetHostViewBase::SetBrowserAccessibilityManager(
    BrowserAccessibilityManager* manager) {
  browser_accessibility_manager_.reset(manager);
}

void RenderWidgetHostViewBase::UpdateScreenInfo() {
  gfx::Display monitor = gfx::Screen::GetMonitorNearestPoint(
      GetViewBounds().origin());
  if (current_display_area_ == monitor.bounds() &&
      current_device_scale_factor_ == monitor.device_scale_factor())
    return;
  current_display_area_ = monitor.bounds();
  current_device_scale_factor_ = monitor.device_scale_factor();
  if (GetRenderWidgetHost()) {
    RenderWidgetHostImpl* impl =
        RenderWidgetHostImpl::From(GetRenderWidgetHost());
    impl->NotifyScreenInfoChanged();
  }
}

}  // namespace content
