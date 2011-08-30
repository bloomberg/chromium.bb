// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "aura/window.h"

#include <algorithm>

#include "aura/desktop.h"
#include "aura/window_delegate.h"
#include "base/logging.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/compositor/layer.h"

namespace aura {

Window::Window(WindowDelegate* delegate)
    : delegate_(delegate),
      visibility_(VISIBILITY_HIDDEN),
      needs_paint_all_(true),
      parent_(NULL),
      id_(-1) {
}

Window::~Window() {
  if (delegate_)
    delegate_->OnWindowDestroyed();
  if (parent_)
    parent_->RemoveChild(this);
}

void Window::Init() {
  layer_.reset(new ui::Layer(Desktop::GetInstance()->compositor()));
}

void Window::SetVisibility(Visibility visibility) {
  if (visibility_ == visibility)
    return;

  visibility_ = visibility;
}

void Window::SetBounds(const gfx::Rect& bounds, int anim_ms) {
  // TODO: support anim_ms
  // TODO: funnel this through the Desktop.
  bounds_ = bounds;
  layer_->SetBounds(bounds);
}

void Window::SchedulePaint(const gfx::Rect& bounds) {
  if (dirty_rect_.IsEmpty())
    dirty_rect_ = bounds;
  else
    dirty_rect_ = dirty_rect_.Union(bounds);
}

void Window::SetCanvas(const SkCanvas& canvas, const gfx::Point& origin) {
  // TODO: figure out how this is going to work when animating the layer. In
  // particular if we're animating the size then the underlying Texture is going
  // to be unhappy if we try to set a texture on a size bigger than the size of
  // the texture.
  layer_->SetCanvas(canvas, origin);
}

void Window::SetParent(Window* parent) {
  if (parent)
    parent->AddChild(this);
  else
    Desktop::GetInstance()->window()->AddChild(this);
}

void Window::DrawTree() {
  UpdateLayerCanvas();
  Draw();

  for (Windows::iterator i = children_.begin(); i != children_.end(); ++i)
    (*i)->DrawTree();
}

void Window::AddChild(Window* child) {
  DCHECK(std::find(children_.begin(), children_.end(), child) ==
      children_.end());
  child->parent_ = this;
  layer_->Add(child->layer_.get());
  children_.push_back(child);
}

void Window::RemoveChild(Window* child) {
  Windows::iterator i = std::find(children_.begin(), children_.end(), child);
  DCHECK(i != children_.end());
  child->parent_ = NULL;
  layer_->Remove(child->layer_.get());
  children_.erase(i);
}

// static
void Window::ConvertPointToWindow(Window* source,
                                  Window* target,
                                  gfx::Point* point) {
  ui::Layer::ConvertPointToLayer(source->layer(), target->layer(), point);
}

bool Window::OnMouseEvent(const MouseEvent& event) {
  return true;
}

bool Window::HitTest(const gfx::Point& point) {
  gfx::Rect local_bounds(gfx::Point(), bounds().size());
  // TODO(beng): hittest masks.
  return local_bounds.Contains(point);
}

Window* Window::GetEventHandlerForPoint(const gfx::Point& point) {
  Windows::const_reverse_iterator i = children_.rbegin();
  for (; i != children_.rend(); ++i) {
    Window* child = *i;
    if (child->visibility() == Window::VISIBILITY_HIDDEN)
      continue;
    gfx::Point point_in_child_coords(point);
    Window::ConvertPointToWindow(this, child, &point_in_child_coords);
    if (child->HitTest(point_in_child_coords))
      return child->GetEventHandlerForPoint(point_in_child_coords);
  }
  return this;
}

void Window::UpdateLayerCanvas() {
  if (needs_paint_all_) {
    needs_paint_all_ = false;
    dirty_rect_ = gfx::Rect(0, 0, bounds().width(), bounds().height());
  }
  gfx::Rect dirty_rect = dirty_rect_.Intersect(
      gfx::Rect(0, 0, bounds().width(), bounds().height()));
  dirty_rect_.SetRect(0, 0, 0, 0);
  if (dirty_rect.IsEmpty())
    return;
  if (delegate_) {
    scoped_ptr<gfx::Canvas> canvas(gfx::Canvas::CreateCanvas(
        dirty_rect.width(), dirty_rect.height(), false));
    canvas->TranslateInt(dirty_rect.x(), dirty_rect.y());
    delegate_->OnPaint(canvas.get());
    SetCanvas(*canvas->AsCanvasSkia(), bounds().origin());
  }
}

void Window::Draw() {
  if (visibility_ != VISIBILITY_HIDDEN)
    layer_->Draw();
}

}  // namespace aura
