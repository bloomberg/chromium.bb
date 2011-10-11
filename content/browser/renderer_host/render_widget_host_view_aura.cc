// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_aura.h"

#include "base/logging.h"
#include "ui/aura/hit_test.h"
#include "ui/aura/window.h"

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, public:

RenderWidgetHostViewAura::RenderWidgetHostViewAura(RenderWidgetHost* host)
    : host_(host) {
}

RenderWidgetHostViewAura::~RenderWidgetHostViewAura() {
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, RenderWidgetHostView implementation:

void RenderWidgetHostViewAura::InitAsPopup(
    RenderWidgetHostView* parent_host_view,
    const gfx::Rect& pos) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::InitAsFullscreen(
    RenderWidgetHostView* reference_host_view) {
  NOTIMPLEMENTED();
}

RenderWidgetHost* RenderWidgetHostViewAura::GetRenderWidgetHost() const {
  return host_;
}

void RenderWidgetHostViewAura::DidBecomeSelected() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::WasHidden() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::SetSize(const gfx::Size& size) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::SetBounds(const gfx::Rect& rect) {
  NOTIMPLEMENTED();
}

gfx::NativeView RenderWidgetHostViewAura::GetNativeView() const {
  return window_;
}

gfx::NativeViewId RenderWidgetHostViewAura::GetNativeViewId() const {
  NOTIMPLEMENTED();
  return NULL;
}

void RenderWidgetHostViewAura::MovePluginWindows(
    const std::vector<webkit::npapi::WebPluginGeometry>& moves) {
  NOTIMPLEMENTED();
}

bool RenderWidgetHostViewAura::HasFocus() {
  NOTIMPLEMENTED();
  return false;
}

void RenderWidgetHostViewAura::Show() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::Hide() {
  NOTIMPLEMENTED();
}

bool RenderWidgetHostViewAura::IsShowing() {
  NOTIMPLEMENTED();
  return false;
}

gfx::Rect RenderWidgetHostViewAura::GetViewBounds() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

void RenderWidgetHostViewAura::UpdateCursor(const WebCursor& cursor) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::SetIsLoading(bool is_loading) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::ImeUpdateTextInputState(
    ui::TextInputType type,
    bool can_compose_inline,
    const gfx::Rect& caret_rect) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::ImeCancelComposition() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::DidUpdateBackingStore(
    const gfx::Rect& scroll_rect, int scroll_dx, int scroll_dy,
    const std::vector<gfx::Rect>& copy_rects) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::RenderViewGone(base::TerminationStatus status,
                                              int error_code) {
  NOTIMPLEMENTED();
}
void RenderWidgetHostViewAura::Destroy() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::SetTooltipText(const string16& tooltip_text) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::SelectionChanged(const std::string& text,
                                                const ui::Range& range,
                                                const gfx::Point& start,
                                                const gfx::Point& end) {
                                                  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::ShowingContextMenu(bool showing) {
  NOTIMPLEMENTED();
}

BackingStore* RenderWidgetHostViewAura::AllocBackingStore(
    const gfx::Size& size) {
  NOTIMPLEMENTED();
  return NULL;
}

void RenderWidgetHostViewAura::SetBackground(const SkBitmap& background) {
  NOTIMPLEMENTED();
}

#if defined(OS_POSIX)
void RenderWidgetHostViewAura::GetDefaultScreenInfo(
    WebKit::WebScreenInfo* results) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::GetScreenInfo(WebKit::WebScreenInfo* results) {
  NOTIMPLEMENTED();
}

gfx::Rect RenderWidgetHostViewAura::GetRootWindowBounds() {
  NOTIMPLEMENTED();
  return gfx::Rect();
}
#endif

void RenderWidgetHostViewAura::SetVisuallyDeemphasized(const SkColor* color,
                                                       bool animate) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::UnhandledWheelEvent(
    const WebKit::WebMouseWheelEvent& event) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::SetHasHorizontalScrollbar(
    bool has_horizontal_scrollbar) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::SetScrollOffsetPinning(
    bool is_pinned_to_left, bool is_pinned_to_right) {
  NOTIMPLEMENTED();
}

#if defined(OS_WIN)
void RenderWidgetHostViewAura::WillWmDestroy() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::ShowCompositorHostWindow(bool show) {
  NOTIMPLEMENTED();
}
#endif

gfx::PluginWindowHandle RenderWidgetHostViewAura::GetCompositingSurface() {
  NOTIMPLEMENTED();
  return NULL;
}

bool RenderWidgetHostViewAura::LockMouse() {
  NOTIMPLEMENTED();
  return false;
}

void RenderWidgetHostViewAura::UnlockMouse() {
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, aura::WindowDelegate implementation:

void RenderWidgetHostViewAura::OnBoundsChanged(const gfx::Rect& old_bounds,
                                               const gfx::Rect& new_bounds) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::OnFocus() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::OnBlur() {
  NOTIMPLEMENTED();
}

bool RenderWidgetHostViewAura::OnKeyEvent(aura::KeyEvent* event) {
  NOTIMPLEMENTED();
  return false;
}

gfx::NativeCursor RenderWidgetHostViewAura::GetCursor(const gfx::Point& point) {
  NOTIMPLEMENTED();
  return NULL;
}

int RenderWidgetHostViewAura::GetNonClientComponent(
    const gfx::Point& point) const {
  NOTIMPLEMENTED();
  return HTCLIENT;
}

bool RenderWidgetHostViewAura::OnMouseEvent(aura::MouseEvent* event) {
  NOTIMPLEMENTED();
  return false;
}

bool RenderWidgetHostViewAura::ShouldActivate(aura::MouseEvent* event) {
  NOTIMPLEMENTED();
  return false;
}

void RenderWidgetHostViewAura::OnActivated() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::OnLostActive() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::OnCaptureLost() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::OnPaint(gfx::Canvas* canvas) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::OnWindowDestroying() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewAura::OnWindowDestroyed() {
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, private:

