// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_H_
#define CHROME_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_H_
#pragma once

#include "base/basictypes.h"

////////////////////////////////////////////////////////////////////////////////
//
// BrowserAccessibility
//
// Class implementing the cross platform interface for the Browser-Renderer
// communication of accessibility information, providing accessibility
// to be used by screen readers and other assistive technology (AT).
//
// An implementation for each platform handles platform specific accessibility
// APIs.
//
////////////////////////////////////////////////////////////////////////////////
class BrowserAccessibility {
 public:
  // Creates the platform specific BrowserAccessibility. Ownership passes to the
  // caller.
  virtual ~BrowserAccessibility();

 protected:
  BrowserAccessibility();

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibility);
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_H_
