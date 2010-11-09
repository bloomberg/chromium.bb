// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DROPDOWN_BAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DROPDOWN_BAR_VIEW_H_
#pragma once

#include "views/view.h"

class DropdownBarHost;

////////////////////////////////////////////////////////////////////////////////
//
// The DropdownBarView is an abstract view to draw the UI controls of the
// DropdownBarHost.
//
////////////////////////////////////////////////////////////////////////////////
class DropdownBarView : public views::View {
 public:
  explicit DropdownBarView(DropdownBarHost* host)
      : host_(host),
        animation_offset_(0) {
  }
  virtual ~DropdownBarView() {}

  // Claims focus for the text field and selects its contents.
  virtual void SetFocusAndSelection(bool select_all) = 0;

  // Updates the view to let it know where the host is clipping the
  // dropdown widget (while animating the opening or closing of the widget).
  void set_animation_offset(int offset) { animation_offset_ = offset; }

  // Returns the offset used while animating.
  int animation_offset() const { return animation_offset_; }

 protected:
  // Returns the DropdownBarHost that manages this view.
  DropdownBarHost* host() const { return host_; }

 private:
  // The dropdown bar host that controls this view.
  DropdownBarHost* host_;

  // While animating, the host clips the widget and draws only the bottom
  // part of it. The view needs to know the pixel offset at which we are drawing
  // the widget so that we can draw the curved edges that attach to the toolbar
  // in the right location.
  int animation_offset_;

  DISALLOW_COPY_AND_ASSIGN(DropdownBarView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_DROPDOWN_BAR_VIEW_H_
