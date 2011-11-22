// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_BUBBLE_BUBBLE_ACCELERATORS_GTK_H_
#define CHROME_BROWSER_UI_GTK_BUBBLE_BUBBLE_ACCELERATORS_GTK_H_
#pragma once

#include <gdk/gdk.h>

#include "base/basictypes.h"

struct BubbleAcceleratorGtk {
  guint keyval;
  GdkModifierType modifier_type;
};

// This class contains a list of accelerators that a BubbleGtk is expected to
// either catch and respond to or catch and forward to the root browser window.
// This list is expected to be a subset of the accelerators that are handled by
// the root browser window, but the specific accelerators to be handled has not
// yet been fully specified.
class BubbleAcceleratorsGtk {
 public:
  typedef const BubbleAcceleratorGtk* const_iterator;

  static const_iterator begin();
  static const_iterator end();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(BubbleAcceleratorsGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_BUBBLE_BUBBLE_ACCELERATORS_GTK_H_
