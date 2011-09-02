// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "aura/window.h"

#include <algorithm>

#include "aura/desktop.h"
#include "aura/event.h"
#include "aura/window_delegate.h"
#include "aura/window_manager.h"
#include "base/logging.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/compositor/layer.h"

namespace aura {

Window::Window(WindowDelegate* delegate)
    : delegate_(delegate),
      visibility_(VISIBILITY_HIDDEN),
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
  layer_->set_delegate(this);
}

void Window::SetVisibility(Visibility visibility) {
  if (visibility_ == visibility)
    return;

  visibility_ = visibility;
  if (visibility_ != VISIBILITY_HIDDEN)
    SchedulePaint();
}

void Window::SetBounds(const gfx::Rect& bounds, int anim_ms) {
  // TODO: support anim_ms
  // TODO: funnel this through the Desktop.
  bool was_move = bounds_.size() == bounds.size();
  bounds_ = bounds;
  layer_->SetBounds(bounds);
  if (was_move)
    SchedulePaintInRect(gfx::Rect());
  else
    SchedulePaint();
}

void Window::SchedulePaintInRect(const gfx::Rect& rect) {
  layer_->SchedulePaint(rect);
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

void Window::MoveChildToFront(Window* child) {
  DCHECK_EQ(child->parent(), this);
  const Windows::iterator i(std::find(children_.begin(), children_.end(),
                                      child));
  DCHECK(i != children_.end());
  children_.erase(i);

  // TODO(beng): this obviously has to handle different window types.
  children_.insert(children_.begin() + children_.size(), child);
  SchedulePaintInRect(gfx::Rect());
}

void Window::DrawTree() {
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

bool Window::OnMouseEvent(MouseEvent* event) {
  if (!window_manager_.get())
    window_manager_.reset(new WindowManager(this));
  return window_manager_->OnMouseEvent(event) || delegate_->OnMouseEvent(event);
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

void Window::Draw() {
  if (visibility_ != VISIBILITY_HIDDEN)
    layer_->Draw();
}

void Window::SchedulePaint() {
  SchedulePaintInRect(gfx::Rect(0, 0, bounds_.width(), bounds_.height()));
}

void Window::OnPaint(gfx::Canvas* canvas) {
  delegate_->OnPaint(canvas);
}

}  // namespace aura
