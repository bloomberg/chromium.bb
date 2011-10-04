// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/tabs/dragged_tab_gtk.h"

#include <gdk/gdk.h>

#include <algorithm>

#include "base/i18n/rtl.h"
#include "base/stl_util.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/tabs/dragged_tab_data.h"
#include "chrome/browser/ui/gtk/tabs/tab_renderer_gtk.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/browser/renderer_host/backing_store_gtk.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/gtk_util.h"

namespace {

// The size of the dragged window frame.
const int kDragFrameBorderSize = 1;
const int kTwiceDragFrameBorderSize = 2 * kDragFrameBorderSize;

// Used to scale the dragged window sizes.
const float kScalingFactor = 0.5;

const int kAnimateToBoundsDurationMs = 150;

const gdouble kTransparentAlpha = (200.0f / 255.0f);
const gdouble kOpaqueAlpha = 1.0f;
const double kDraggedTabBorderColor[] = { 103.0 / 0xff,
                                          129.0 / 0xff,
                                          162.0 / 0xff };

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DraggedTabGtk, public:

DraggedTabGtk::DraggedTabGtk(DragData* drag_data,
                             const gfx::Point& mouse_tab_offset,
                             const gfx::Size& contents_size)
    : drag_data_(drag_data),
      mini_width_(-1),
      normal_width_(-1),
      attached_(false),
      parent_window_width_(-1),
      mouse_tab_offset_(mouse_tab_offset),
      attached_tab_size_(TabRendererGtk::GetMinimumSelectedSize()),
      contents_size_(contents_size),
      close_animation_(this) {
  std::vector<TabContents*> data_sources(drag_data_->GetDraggedTabsContents());
  for (size_t i = 0; i < data_sources.size(); i++) {
    renderers_.push_back(new TabRendererGtk(ThemeServiceFactory::GetForProfile(
        Profile::FromBrowserContext(data_sources[i]->browser_context()))));
  }

  for (size_t i = 0; i < drag_data_->size(); i++) {
    TabContentsWrapper* wrapper =
        TabContentsWrapper::GetCurrentWrapperForContents(
            drag_data_->get(i)->contents_->tab_contents());
    renderers_[i]->UpdateData(
        drag_data_->get(i)->contents_->tab_contents(),
        wrapper->extension_tab_helper()->is_app(),
        false); // loading_only
    renderers_[i]->set_is_active(
        static_cast<int>(i) == drag_data_->source_tab_index());
  }

  container_ = gtk_window_new(GTK_WINDOW_POPUP);
  SetContainerColorMap();
  gtk_widget_set_app_paintable(container_, TRUE);
  g_signal_connect(container_, "expose-event", G_CALLBACK(OnExposeThunk), this);
  gtk_widget_add_events(container_, GDK_STRUCTURE_MASK);

  // We contain the tab renderer in a GtkFixed in order to maintain the
  // requested size.  Otherwise, the widget will fill the entire window and
  // cause a crash when rendering because the bounds don't match our images.
  fixed_ = gtk_fixed_new();
  for (size_t i = 0; i < renderers_.size(); i++)
    gtk_fixed_put(GTK_FIXED(fixed_), renderers_[i]->widget(), 0, 0);

  gtk_container_add(GTK_CONTAINER(container_), fixed_);
  gtk_widget_show_all(container_);
}

DraggedTabGtk::~DraggedTabGtk() {
  gtk_widget_destroy(container_);
  STLDeleteElements(&renderers_);
}

void DraggedTabGtk::MoveDetachedTo(const gfx::Point& screen_point) {
  DCHECK(!attached_);
  gfx::Point distance_from_origin =
      GetDistanceFromTabStripOriginToMousePointer();
  int y = screen_point.y() - ScaleValue(distance_from_origin.y());
  int x = screen_point.x() - ScaleValue(distance_from_origin.x());
  gtk_window_move(GTK_WINDOW(container_), x, y);
}

void DraggedTabGtk::MoveAttachedTo(const gfx::Point& tabstrip_point) {
  DCHECK(attached_);
  int x = tabstrip_point.x() + GetWidthInTabStripUpToMousePointer() -
      ScaleValue(GetWidthInTabStripUpToMousePointer());
  int y = tabstrip_point.y() + mouse_tab_offset_.y() -
      ScaleValue(mouse_tab_offset_.y());
  gtk_window_move(GTK_WINDOW(container_), x, y);
}

gfx::Point DraggedTabGtk::GetDistanceFromTabStripOriginToMousePointer() {
  gfx::Point start_point(GetWidthInTabStripUpToMousePointer(),
                         mouse_tab_offset_.y());
  if (base::i18n::IsRTL())
    start_point.Offset(parent_window_width_ - GetTotalWidthInTabStrip(), 0);
  return start_point;
}

void DraggedTabGtk::Attach(int normal_width, int mini_width, int window_width) {
  attached_ = true;
  parent_window_width_ = window_width;
  normal_width_ = normal_width;
  mini_width_ = mini_width;

  int dragged_tab_width =
      drag_data_->GetSourceTabData()->mini_ ? mini_width : normal_width;

  Resize(dragged_tab_width);

  if (gtk_util::IsScreenComposited())
    gdk_window_set_opacity(container_->window, kOpaqueAlpha);
}

void DraggedTabGtk::Resize(int width) {
  attached_tab_size_.set_width(width);
  ResizeContainer();
}

void DraggedTabGtk::Detach() {
  attached_ = false;
  ResizeContainer();

  if (gtk_util::IsScreenComposited())
    gdk_window_set_opacity(container_->window, kTransparentAlpha);
}

void DraggedTabGtk::Update() {
  gtk_widget_queue_draw(container_);
}

int DraggedTabGtk::GetWidthInTabStripFromTo(int from, int to) {
  DCHECK(from <= static_cast<int>(drag_data_->size()));
  DCHECK(to <= static_cast<int>(drag_data_->size()));

  // TODO(dpapad): Get 16 from TabStripGtk::kTabHOffset.
  int mini_tab_count = 0, non_mini_tab_count = 0;
  drag_data_->GetNumberOfMiniNonMiniTabs(from, to,
                                         &mini_tab_count, &non_mini_tab_count);
  int width = non_mini_tab_count * static_cast<int>(floor(normal_width_ + 0.5))
      + mini_tab_count * mini_width_ - std::max(to - from - 1, 0) * 16;
  return width;
}

int DraggedTabGtk::GetTotalWidthInTabStrip() {
  return GetWidthInTabStripFromTo(0, drag_data_->size());
}

int DraggedTabGtk::GetWidthInTabStripUpToSourceTab() {
  if (!base::i18n::IsRTL()) {
    return GetWidthInTabStripFromTo(0, drag_data_->source_tab_index());
  } else {
    return GetWidthInTabStripFromTo(
        drag_data_->source_tab_index() + 1, drag_data_->size());
  }
}

int DraggedTabGtk::GetWidthInTabStripUpToMousePointer() {
   int width = GetWidthInTabStripUpToSourceTab() + mouse_tab_offset_.x();
   if (!base::i18n::IsRTL() && drag_data_->source_tab_index() > 0) {
     width -= 16;
   } else if (base::i18n::IsRTL() &&
              drag_data_->source_tab_index() <
                  static_cast<int>(drag_data_->size()) - 1) {
     width -= 16;
   }
   return width;
}

void DraggedTabGtk::AnimateToBounds(const gfx::Rect& bounds,
                                    AnimateToBoundsCallback* callback) {
  animation_callback_.reset(callback);

  gint x, y, width, height;
  gdk_window_get_origin(container_->window, &x, &y);
  gdk_window_get_geometry(container_->window, NULL, NULL,
                          &width, &height, NULL);

  animation_start_bounds_ = gfx::Rect(x, y, width, height);
  animation_end_bounds_ = bounds;

  close_animation_.SetSlideDuration(kAnimateToBoundsDurationMs);
  close_animation_.SetTweenType(ui::Tween::EASE_OUT);
  if (!close_animation_.IsShowing()) {
    close_animation_.Reset();
    close_animation_.Show();
  }
}

////////////////////////////////////////////////////////////////////////////////
// DraggedTabGtk, ui::AnimationDelegate implementation:

void DraggedTabGtk::AnimationProgressed(const ui::Animation* animation) {
  int delta_x = (animation_end_bounds_.x() - animation_start_bounds_.x());
  int x = animation_start_bounds_.x() +
      static_cast<int>(delta_x * animation->GetCurrentValue());
  int y = animation_end_bounds_.y();
  gdk_window_move(container_->window, x, y);
}

void DraggedTabGtk::AnimationEnded(const ui::Animation* animation) {
  animation_callback_->Run();
}

void DraggedTabGtk::AnimationCanceled(const ui::Animation* animation) {
  AnimationEnded(animation);
}

////////////////////////////////////////////////////////////////////////////////
// DraggedTabGtk, private:

void DraggedTabGtk::Layout() {
  if (attached_) {
    for (size_t i = 0; i < renderers_.size(); i++) {
      gfx::Rect rect(GetPreferredSize());
      rect.set_width(GetAttachedTabWidthAt(i));
      renderers_[i]->SetBounds(rect);
    }
  } else {
    int left = 0;
    if (base::i18n::IsRTL())
      left = GetPreferredSize().width() - attached_tab_size_.width();

    // The renderer_'s width should be attached_tab_size_.width() in both LTR
    // and RTL locales. Wrong width will cause the wrong positioning of the tab
    // view in dragging. Please refer to http://crbug.com/6223 for details.
    renderers_[drag_data_->source_tab_index()]->SetBounds(
        gfx::Rect(left, 0, attached_tab_size_.width(),
                  attached_tab_size_.height()));
  }
}

gfx::Size DraggedTabGtk::GetPreferredSize() {
  if (attached_) {
    gfx::Size preferred_size(attached_tab_size_);
    preferred_size.set_width(GetTotalWidthInTabStrip());
    return preferred_size;
  }

  int width = std::max(attached_tab_size_.width(), contents_size_.width()) +
      kTwiceDragFrameBorderSize;
  int height = attached_tab_size_.height() + kDragFrameBorderSize +
      contents_size_.height();
  return gfx::Size(width, height);
}

void DraggedTabGtk::ResizeContainer() {
  gfx::Size size = GetPreferredSize();
  gtk_window_resize(GTK_WINDOW(container_),
                    ScaleValue(size.width()), ScaleValue(size.height()));
  Layout();
}

int DraggedTabGtk::ScaleValue(int value) {
  return attached_ ? value : static_cast<int>(value * kScalingFactor);
}

gfx::Rect DraggedTabGtk::bounds() const {
  gint x, y, width, height;
  gtk_window_get_position(GTK_WINDOW(container_), &x, &y);
  gtk_window_get_size(GTK_WINDOW(container_), &width, &height);
  return gfx::Rect(x, y, width, height);
}

int DraggedTabGtk::GetAttachedTabWidthAt(int index) {
  return drag_data_->get(index)->mini_? mini_width_ : normal_width_;
}

void DraggedTabGtk::SetContainerColorMap() {
  GdkScreen* screen = gtk_widget_get_screen(container_);
  GdkColormap* colormap = gdk_screen_get_rgba_colormap(screen);

  // If rgba is not available, use rgb instead.
  if (!colormap)
    colormap = gdk_screen_get_rgb_colormap(screen);

  gtk_widget_set_colormap(container_, colormap);
}

void DraggedTabGtk::SetContainerTransparency() {
  cairo_t* cairo_context = gdk_cairo_create(container_->window);
  if (!cairo_context)
    return;

  // Make the background of the dragged tab window fully transparent.  All of
  // the content of the window (child widgets) will be completely opaque.
  gfx::Size size = bounds().size();
  cairo_scale(cairo_context, static_cast<double>(size.width()),
              static_cast<double>(size.height()));
  cairo_set_source_rgba(cairo_context, 1.0f, 1.0f, 1.0f, 0.0f);
  cairo_set_operator(cairo_context, CAIRO_OPERATOR_SOURCE);
  cairo_paint(cairo_context);
  cairo_destroy(cairo_context);
}

void DraggedTabGtk::SetContainerShapeMask() {
  // Create a 1bpp bitmap the size of |container_|.
  gfx::Size size(GetPreferredSize());
  GdkPixmap* pixmap = gdk_pixmap_new(NULL, size.width(), size.height(), 1);
  cairo_t* cairo_context = gdk_cairo_create(GDK_DRAWABLE(pixmap));

  // Set the transparency.
  cairo_set_source_rgba(cairo_context, 1.0f, 1.0f, 1.0f, 0.0f);

  // Blit the rendered bitmap into a pixmap.  Any pixel set in the pixmap will
  // be opaque in the container window.
  if (!attached_)
    cairo_scale(cairo_context, kScalingFactor, kScalingFactor);
  for (size_t i = 0; i < renderers_.size(); i++) {
    if (static_cast<int>(i) == 0)
      cairo_set_operator(cairo_context, CAIRO_OPERATOR_SOURCE);
    else
      cairo_set_operator(cairo_context, CAIRO_OPERATOR_OVER);
    PaintTab(i, cairo_context, container_->allocation.width);
  }

  if (!attached_) {
    // Make the render area depiction opaque (leaving enough room for the
    // border).
    cairo_identity_matrix(cairo_context);
    // On Lucid running VNC, the X server will reject RGBA (1,1,1,1) as an
    // invalid value below in gdk_window_shape_combine_mask(). Using (0,0,0,1)
    // instead. The value doesn't really matter, as long as the alpha is not 0.
    cairo_set_source_rgba(cairo_context, 0.0f, 0.0f, 0.0f, 1.0f);
    int tab_height = static_cast<int>(
        kScalingFactor * renderers_[drag_data_->source_tab_index()]->height() -
            kDragFrameBorderSize);
    cairo_rectangle(cairo_context,
                    0, tab_height,
                    size.width(), size.height() - tab_height);
    cairo_fill(cairo_context);
  }

  cairo_destroy(cairo_context);

  // Set the shape mask.
  gdk_window_shape_combine_mask(container_->window, pixmap, 0, 0);
  g_object_unref(pixmap);
}

gboolean DraggedTabGtk::OnExpose(GtkWidget* widget, GdkEventExpose* event) {
  if (gtk_util::IsScreenComposited())
    SetContainerTransparency();
  else
    SetContainerShapeMask();

  // Only used when not attached.
  int tab_height = static_cast<int>(
      kScalingFactor * renderers_[drag_data_->source_tab_index()]->height());

  // Draw the render area.
  BackingStore* backing_store = drag_data_->GetSourceTabContents()->
      render_view_host()->GetBackingStore(false);
  if (backing_store && !attached_) {
    // This leaves room for the border.
    static_cast<BackingStoreGtk*>(backing_store)->PaintToRect(
        gfx::Rect(kDragFrameBorderSize, tab_height,
                  widget->allocation.width - kTwiceDragFrameBorderSize,
                  widget->allocation.height - tab_height -
                  kDragFrameBorderSize),
        GDK_DRAWABLE(widget->window));
  }

  cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(widget->window));
  // Draw the border.
  if (!attached_) {
    cairo_set_line_width(cr, kDragFrameBorderSize);
    cairo_set_source_rgb(cr, kDraggedTabBorderColor[0],
                             kDraggedTabBorderColor[1],
                             kDraggedTabBorderColor[2]);
    // |offset| is the distance from the edge of the image to the middle of
    // the border line.
    double offset = kDragFrameBorderSize / 2.0 - 0.5;
    double left_x = offset;
    double top_y = tab_height - kDragFrameBorderSize + offset;
    double right_x = widget->allocation.width - offset;
    double bottom_y = widget->allocation.height - offset;

    cairo_move_to(cr, left_x, top_y);
    cairo_line_to(cr, left_x, bottom_y);
    cairo_line_to(cr, right_x, bottom_y);
    cairo_line_to(cr, right_x, top_y);
    cairo_line_to(cr, left_x, top_y);
    cairo_stroke(cr);
  }

  // Draw the tab.
  if (!attached_)
    cairo_scale(cr, kScalingFactor, kScalingFactor);
  // Painting all but the active tab first, from last to first.
  for (int i = renderers_.size() - 1; i >= 0; i--) {
    if (i == drag_data_->source_tab_index())
      continue;
    PaintTab(i, cr, widget->allocation.width);
  }
  // Painting the active tab last, so that it appears on top.
  PaintTab(drag_data_->source_tab_index(), cr, widget->allocation.width);

  cairo_destroy(cr);

  // We've already drawn the tab, so don't propagate the expose-event signal.
  return TRUE;
}

void DraggedTabGtk::PaintTab(int index, cairo_t* cr, int widget_width) {
  renderers_[index]->set_mini(drag_data_->get(index)->mini_);
  cairo_surface_t* surface = renderers_[index]->PaintToSurface();

  int paint_at = 0;
  if (!base::i18n::IsRTL()) {
    paint_at = std::max(GetWidthInTabStripFromTo(0, index) - 16, 0);
  } else {
    paint_at = GetTotalWidthInTabStrip() -
        GetWidthInTabStripFromTo(0, index + 1);
    if (!attached_) {
      paint_at = widget_width / kScalingFactor -
          GetWidthInTabStripFromTo(0, index + 1);
    }
  }

  cairo_set_source_surface(cr, surface, paint_at, 0);
  cairo_paint(cr);
  cairo_surface_destroy(surface);
}
