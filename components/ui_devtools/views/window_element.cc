// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/window_element.h"

#include "components/ui_devtools/views/ui_element_delegate.h"
#include "ui/aura/window.h"

namespace ui_devtools {
namespace {

int GetIndexOfChildInParent(aura::Window* window) {
  const aura::Window::Windows& siblings = window->parent()->children();
  auto it = std::find(siblings.begin(), siblings.end(), window);
  DCHECK(it != siblings.end());
  return std::distance(siblings.begin(), it);
}

}  // namespace

WindowElement::WindowElement(aura::Window* window,
                             UIElementDelegate* ui_element_delegate,
                             UIElement* parent)
    : UIElement(UIElementType::WINDOW, ui_element_delegate, parent),
      window_(window) {
  if (window)
    window_->AddObserver(this);
}

WindowElement::~WindowElement() {
  if (window_)
    window_->RemoveObserver(this);
}

// Handles removing window_.
void WindowElement::OnWindowHierarchyChanging(
    const aura::WindowObserver::HierarchyChangeParams& params) {
  if (params.target == window_) {
    parent()->RemoveChild(this);
    delete this;
  }
}

// Handles adding window_.
void WindowElement::OnWindowHierarchyChanged(
    const aura::WindowObserver::HierarchyChangeParams& params) {
  if (window_ == params.new_parent && params.receiver == params.new_parent) {
    AddChild(new WindowElement(params.target, delegate(), this),
             children().empty() ? nullptr : children().back());
  }
}

void WindowElement::OnWindowStackingChanged(aura::Window* window) {
  DCHECK_EQ(window_, window);
  parent()->ReorderChild(this, GetIndexOfChildInParent(window));
}

void WindowElement::OnWindowBoundsChanged(aura::Window* window,
                                          const gfx::Rect& old_bounds,
                                          const gfx::Rect& new_bounds,
                                          ui::PropertyChangeReason reason) {
  DCHECK_EQ(window_, window);
  delegate()->OnUIElementBoundsChanged(this);
}

std::vector<std::pair<std::string, std::string>>
WindowElement::GetCustomAttributes() const {
  return {};
}

void WindowElement::GetBounds(gfx::Rect* bounds) const {
  *bounds = window_->bounds();
}

void WindowElement::SetBounds(const gfx::Rect& bounds) {
  window_->SetBounds(bounds);
}

void WindowElement::GetVisible(bool* visible) const {
  *visible = window_->IsVisible();
}

void WindowElement::SetVisible(bool visible) {
  if (visible)
    window_->Show();
  else
    window_->Hide();
}

std::pair<aura::Window*, gfx::Rect> WindowElement::GetNodeWindowAndBounds()
    const {
  return std::make_pair(window_, window_->GetBoundsInScreen());
}

// static
aura::Window* WindowElement::From(const UIElement* element) {
  DCHECK_EQ(UIElementType::WINDOW, element->type());
  return static_cast<const WindowElement*>(element)->window_;
}

}  // namespace ui_devtools
