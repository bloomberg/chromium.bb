// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/ui_element.h"

#include <algorithm>

#include "components/ui_devtools/views/ui_element_delegate.h"
#include "components/ui_devtools/views/view_element.h"
#include "components/ui_devtools/views/widget_element.h"
#include "components/ui_devtools/views/window_element.h"

namespace ui_devtools {
namespace {

static int node_ids = 0;

}  // namespace

UIElement::~UIElement() {
  for (auto* child : children_)
    delete child;
  children_.clear();
}

std::string UIElement::GetTypeName() const {
  switch (type_) {
    case UIElementType::WINDOW:
      return "Window";
    case UIElementType::WIDGET:
      return "Widget";
    case UIElementType::VIEW:
      return "View";
  }
  NOTREACHED();
  return std::string();
}

void UIElement::AddChild(UIElement* child, UIElement* before) {
  if (before) {
    auto iter = std::find(children_.begin(), children_.end(), before);
    DCHECK(iter != children_.end());
    children_.insert(iter, child);
  } else {
    children_.push_back(child);
  }
  delegate_->OnUIElementAdded(this, child);
}

void UIElement::RemoveChild(UIElement* child) {
  delegate()->OnUIElementRemoved(child);
  auto iter = std::find(children_.begin(), children_.end(), child);
  DCHECK(iter != children_.end());
  children_.erase(iter);
}

void UIElement::ReorderChild(UIElement* child, int new_index) {
  // Remove |child| out of vector |children_|.
  auto iter = std::find(children_.begin(), children_.end(), child);
  DCHECK(iter != children_.end());
  children_.erase(iter);

  // Move child to new position |new_index| in vector |children_|.
  new_index = std::min(static_cast<int>(children_.size()) - 1, new_index);
  iter = children_.begin() + new_index;
  children_.insert(iter, child);
  delegate()->OnUIElementReordered(child->parent(), child);
}

UIElement::UIElement(const UIElementType type,
                     UIElementDelegate* delegate,
                     UIElement* parent)
    : node_id_(++node_ids), type_(type), parent_(parent), delegate_(delegate) {
  delegate_->OnUIElementAdded(0, this);
}

}  // namespace ui_devtools
