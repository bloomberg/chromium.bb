// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CSS_COLORS_H_
#define CONTENT_COMMON_CSS_COLORS_H_
#pragma once

#include <utility>

#include "base/basictypes.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebColor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebColorName.h"

// Functionality related to sending the values of CSS colors to the renderer.
class CSSColors {
 public:
  typedef WebKit::WebColorName CSSColorName;
  typedef std::pair<CSSColorName, WebKit::WebColor> CSSColorMapping;

 private:
  DISALLOW_COPY_AND_ASSIGN(CSSColors);
};

#endif  // CONTENT_COMMON_CSS_COLORS_H_
