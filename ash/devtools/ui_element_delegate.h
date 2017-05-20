// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DEVTOOLS_UI_ELEMENT_DELEGATE_H_
#define ASH_DEVTOOLS_UI_ELEMENT_DELEGATE_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/aura/window.h"

namespace ash {
namespace devtools {

class UIElement;

class ASH_EXPORT UIElementDelegate {
 public:
  UIElementDelegate(){};
  virtual ~UIElementDelegate(){};

  virtual void OnUIElementAdded(UIElement* parent, UIElement* child) = 0;

  // Move |child| to different sibling index under |parent| in DOM tree.
  virtual void OnUIElementReordered(UIElement* parent, UIElement* child) = 0;

  // Remove ui_element in DOM tree.
  virtual void OnUIElementRemoved(UIElement* ui_element) = 0;

  // Update CSS agent when bounds change.
  virtual void OnUIElementBoundsChanged(UIElement* ui_element) = 0;

  // Return |window| highlighting status.
  virtual bool IsHighlightingWindow(aura::Window* window) = 0;

  DISALLOW_COPY_AND_ASSIGN(UIElementDelegate);
};

}  // namespace devtools
}  // namespace ash

#endif  // ASH_DEVTOOLS_UI_ELEMENT_DELEGATE_H_
