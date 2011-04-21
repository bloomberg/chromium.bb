// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_BUBBLE_BUBBLE_ACCELERATORS_GTK_H_
#define CHROME_BROWSER_UI_GTK_BUBBLE_BUBBLE_ACCELERATORS_GTK_H_
#pragma once

#include <gdk/gdktypes.h>
#include <glib.h>

#include <vector>

#include "base/basictypes.h"

struct BubbleAcceleratorGtk {
  guint keyval;
  GdkModifierType modifier_type;
};

typedef std::vector<struct BubbleAcceleratorGtk>
    BubbleAcceleratorGtkList;

// This class contains a list of accelerators that a BubbleGtk is expected to
// either catch and respond to or catch and forward to the root browser window.
// This list is expected to be a subset of the accelerators that are handled by
// the root browser window, but the specific accelerators to be handled has not
// yet been fully specified. The common use case for this class has code that
// uses it needing the entire list and not needing extra processing, so the only
// get method gives you the entire list.
class BubbleAcceleratorsGtk {
 public:
  static BubbleAcceleratorGtkList GetList();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(BubbleAcceleratorsGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_BUBBLE_BUBBLE_ACCELERATORS_GTK_H_
