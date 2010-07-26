// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CSS_COLORS_H_
#define CHROME_COMMON_CSS_COLORS_H_
#pragma once

#include <utility>

#include "base/basictypes.h"
#include "third_party/WebKit/WebKit/chromium/public/WebColor.h"
#include "third_party/WebKit/WebKit/chromium/public/WebColorName.h"

// Functionality related to sending the values of CSS colors to the renderer.
class CSSColors {
 public:
  typedef WebKit::WebColorName CSSColorName;
  typedef std::pair<CSSColorName, WebKit::WebColor> CSSColorMapping;

 private:
  DISALLOW_COPY_AND_ASSIGN(CSSColors);
};

#endif  // CHROME_COMMON_CSS_COLORS_H_

