// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/frame/non_client_frame_view_mash.h"

#include <algorithm>
#include <vector>

#include "base/macros.h"
#include "components/mus/public/cpp/window.h"
#include "grit/mash_wm_resources.h"
#include "mash/wm/frame/caption_buttons/frame_caption_button_container_view.h"
#include "mash/wm/frame/default_header_painter.h"
#include "mash/wm/frame/frame_border_hit_test_controller.h"
#include "mash/wm/frame/header_painter.h"
#include "mojo/converters/input_events/input_events_type_converters.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace mash {
namespace wm {

///////////////////////////////////////////////////////////////////////////////
// NonClientFrameViewMash::HeaderView

// View which paints the header.
class NonClientFrameViewMash::HeaderView : public views::View {
 public:
  // |frame| is the widget that the caption buttons act on.
  explicit HeaderView(views::Widget* frame);
  ~HeaderView() override;

  // Schedules a repaint for the entire title.
  void SchedulePaintForTitle();

  // Tells the window controls to reset themselves to the normal state.
  void ResetWindowControls();

  // Returns the view's preferred height.
  int GetPreferredHeight() const;

  // Returns the view's minimum width.
  int GetMinimumWidth() const;

  void SizeConstraintsChanged();

  void SetFrameColors(SkColor active_frame_color, SkColor inactive_frame_color);

  // views::View:
  void Layout() override;
  void OnPaint(gfx::Canvas* canvas) override;
  void ChildPreferredSizeChanged(views::View* child) override;

  FrameCaptionButtonContainerView* caption_button_container() {
    return caption_button_container_;
  }

 private:
  // The widget that the caption buttons act on.
  views::Widget* frame_;

  // Helper for painting the header.
  scoped_ptr<DefaultHeaderPainter> header_painter_;

  // View which contains the window caption buttons.
  FrameCaptionButtonContainerView* caption_button_container_;

  DISALLOW_COPY_AND_ASSIGN(HeaderView);
};

NonClientFrameViewMash::HeaderView::HeaderView(views::Widget* frame)
    : frame_(frame),
      header_painter_(new DefaultHeaderPainter),
      caption_button_container_(nullptr) {
  caption_button_container_ = new FrameCaptionButtonContainerView(frame_);
  caption_button_container_->UpdateSizeButtonVisibility();
  AddChildView(caption_button_container_);

  header_painter_->Init(frame_, this, caption_button_container_);
}

NonClientFrameViewMash::HeaderView::~HeaderView() {}

void NonClientFrameViewMash::HeaderView::SchedulePaintForTitle() {
  header_painter_->SchedulePaintForTitle();
}

void NonClientFrameViewMash::HeaderView::ResetWindowControls() {
  caption_button_container_->ResetWindowControls();
}

int NonClientFrameViewMash::HeaderView::GetPreferredHeight() const {
  return header_painter_->GetHeaderHeightForPainting();
}

int NonClientFrameViewMash::HeaderView::GetMinimumWidth() const {
  return header_painter_->GetMinimumHeaderWidth();
}

void NonClientFrameViewMash::HeaderView::SizeConstraintsChanged() {
  caption_button_container_->ResetWindowControls();
  caption_button_container_->UpdateSizeButtonVisibility();
  Layout();
}

void NonClientFrameViewMash::HeaderView::SetFrameColors(
    SkColor active_frame_color,
    SkColor inactive_frame_color) {
  header_painter_->SetFrameColors(active_frame_color, inactive_frame_color);
}

///////////////////////////////////////////////////////////////////////////////
// NonClientFrameViewMash::HeaderView, views::View overrides:

void NonClientFrameViewMash::HeaderView::Layout() {
  header_painter_->LayoutHeader();
}

void NonClientFrameViewMash::HeaderView::OnPaint(gfx::Canvas* canvas) {
  bool paint_as_active =
      frame_->non_client_view()->frame_view()->ShouldPaintAsActive();
  caption_button_container_->SetPaintAsActive(paint_as_active);

  HeaderPainter::Mode header_mode = paint_as_active
                                        ? HeaderPainter::MODE_ACTIVE
                                        : HeaderPainter::MODE_INACTIVE;
  header_painter_->PaintHeader(canvas, header_mode);
}

void NonClientFrameViewMash::HeaderView::ChildPreferredSizeChanged(
    views::View* child) {
  // FrameCaptionButtonContainerView animates the visibility changes in
  // UpdateSizeButtonVisibility(false). Due to this a new size is not available
  // until the completion of the animation. Layout in response to the preferred
  // size changes.
  if (child != caption_button_container_)
    return;
  parent()->Layout();
}

////////////////////////////////////////////////////////////////////////////////
// NonClientFrameViewMash, public:

// static
const char NonClientFrameViewMash::kViewClassName[] = "NonClientFrameViewMash";

NonClientFrameViewMash::NonClientFrameViewMash(views::Widget* frame,
                                               mus::Window* window)
    : frame_(frame), window_(window), header_view_(new HeaderView(frame)) {
  // |header_view_| is set as the non client view's overlay view so that it can
  // overlay the web contents in immersive fullscreen.
  AddChildView(header_view_);
  window_->AddObserver(this);
}

NonClientFrameViewMash::~NonClientFrameViewMash() {
  if (window_)
    window_->RemoveObserver(this);
}

// static
gfx::Insets NonClientFrameViewMash::GetPreferredClientAreaInsets() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const int header_height =
      rb.GetImageSkiaNamed(IDR_MASH_WM_WINDOW_CONTROL_BACKGROUND_P)
          ->size()
          .height();
  return gfx::Insets(header_height, 0, 0, 0);
}

// static
int NonClientFrameViewMash::GetMaxTitleBarButtonWidth() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetImageSkiaNamed(IDR_MASH_WM_WINDOW_CONTROL_BACKGROUND_P)
             ->size()
             .width() *
         3;
}

void NonClientFrameViewMash::SetFrameColors(SkColor active_frame_color,
                                            SkColor inactive_frame_color) {
  header_view_->SetFrameColors(active_frame_color, inactive_frame_color);
}

////////////////////////////////////////////////////////////////////////////////
// NonClientFrameViewMash, views::NonClientFrameView overrides:

gfx::Rect NonClientFrameViewMash::GetBoundsForClientView() const {
  gfx::Rect result(GetLocalBounds());
  result.Inset(window_->client_area());
  return result;
}

gfx::Rect NonClientFrameViewMash::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  gfx::Rect window_bounds = client_bounds;
  window_bounds.Inset(
      window_->client_area().left(), window_->client_area().top(),
      window_->client_area().right(), window_->client_area().bottom());
  return window_bounds;
}

int NonClientFrameViewMash::NonClientHitTest(const gfx::Point& point) {
  return FrameBorderHitTestController::NonClientHitTest(
      this, header_view_->caption_button_container(), point);
}

void NonClientFrameViewMash::GetWindowMask(const gfx::Size& size,
                                           gfx::Path* window_mask) {}

void NonClientFrameViewMash::ResetWindowControls() {
  header_view_->ResetWindowControls();
}

void NonClientFrameViewMash::UpdateWindowIcon() {}

void NonClientFrameViewMash::UpdateWindowTitle() {
  header_view_->SchedulePaintForTitle();
}

void NonClientFrameViewMash::SizeConstraintsChanged() {
  header_view_->SizeConstraintsChanged();
}

////////////////////////////////////////////////////////////////////////////////
// NonClientFrameViewMash, views::View overrides:

void NonClientFrameViewMash::Layout() {
  header_view_->SetBounds(0, 0, width(), header_view_->GetPreferredHeight());
  header_view_->Layout();
}

gfx::Size NonClientFrameViewMash::GetPreferredSize() const {
  gfx::Size pref = frame_->client_view()->GetPreferredSize();
  return frame_->non_client_view()
      ->GetWindowBoundsForClientBounds(gfx::Rect(pref))
      .size();
}

const char* NonClientFrameViewMash::GetClassName() const {
  return kViewClassName;
}

gfx::Size NonClientFrameViewMash::GetMinimumSize() const {
  gfx::Size min_client_view_size(frame_->client_view()->GetMinimumSize());
  return gfx::Size(
      std::max(header_view_->GetMinimumWidth(), min_client_view_size.width()),
      NonClientTopBorderHeight() + min_client_view_size.height());
}

gfx::Size NonClientFrameViewMash::GetMaximumSize() const {
  gfx::Size max_client_size(frame_->client_view()->GetMaximumSize());
  int width = 0;
  int height = 0;

  if (max_client_size.width() > 0)
    width = std::max(header_view_->GetMinimumWidth(), max_client_size.width());
  if (max_client_size.height() > 0)
    height = NonClientTopBorderHeight() + max_client_size.height();

  return gfx::Size(width, height);
}

void NonClientFrameViewMash::OnPaint(gfx::Canvas* canvas) {
  canvas->Save();
  NonClientFrameView::OnPaint(canvas);
  canvas->Restore();

  // The client app draws the client area. Make ours totally transparent so
  // we only see the client apps client area.
  canvas->FillRect(GetBoundsForClientView(), SK_ColorBLACK,
                   SkXfermode::kSrc_Mode);
}

void NonClientFrameViewMash::PaintChildren(const ui::PaintContext& context) {
  NonClientFrameView::PaintChildren(context);

  // The client app draws the client area. Make ours totally transparent so
  // we only see the client apps client area.
  ui::PaintRecorder recorder(context, size(), &paint_cache_);
  recorder.canvas()->FillRect(GetBoundsForClientView(), SK_ColorBLACK,
                              SkXfermode::kSrc_Mode);
}

void NonClientFrameViewMash::OnWindowClientAreaChanged(
    mus::Window* window,
    const gfx::Insets& old_client_area,
    const std::vector<gfx::Rect>& old_additional_client_area) {
  // Only the insets effect the rendering.
  if (old_client_area == window->client_area())
    return;

  Layout();
  // NonClientView (our parent) positions the client view based on bounds from
  // us. We need to layout from parent to trigger a layout of the client view.
  if (parent())
    parent()->Layout();
  SchedulePaint();
}

void NonClientFrameViewMash::OnWindowDestroyed(mus::Window* window) {
  window_->RemoveObserver(this);
  window_ = nullptr;
}

views::View* NonClientFrameViewMash::GetHeaderView() {
  return header_view_;
}

////////////////////////////////////////////////////////////////////////////////
// NonClientFrameViewMash, private:

int NonClientFrameViewMash::NonClientTopBorderHeight() const {
  return header_view_->GetPreferredHeight();
}

}  // namespace wm
}  // namespace mash
