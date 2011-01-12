// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOUCH_FRAME_KEYBOARD_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_TOUCH_FRAME_KEYBOARD_CONTAINER_VIEW_H_
#pragma once

#include "views/view.h"

class DOMView;
class Profile;

// A class that contains and decorates the virtual keyboard.
//
// This class is also responsible for managing focus of all views related to
// the keyboard to prevent them from interfering with the ClientView.
class KeyboardContainerView : public views::View {
 public:
  explicit KeyboardContainerView(Profile* profile);
  virtual ~KeyboardContainerView();

  // Overridden from views::View
  virtual void Layout();

 protected:
  // Overridden from views::View
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);

 private:
  DOMView* dom_view_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardContainerView);
};

#endif  // CHROME_BROWSER_UI_TOUCH_FRAME_KEYBOARD_CONTAINER_VIEW_H_
