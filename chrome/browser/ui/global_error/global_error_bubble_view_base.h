// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_ERROR_GLOBAL_ERROR_BUBBLE_VIEW_BASE_H_
#define CHROME_BROWSER_UI_GLOBAL_ERROR_GLOBAL_ERROR_BUBBLE_VIEW_BASE_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"

class Browser;
class GlobalError;

class GlobalErrorBubbleViewBase {
 public:
  static GlobalErrorBubbleViewBase* ShowBubbleView(
      Browser* browser,
      const base::WeakPtr<GlobalError>& error);

  virtual ~GlobalErrorBubbleViewBase() {}

  // Close the bubble view.
  virtual void CloseBubbleView() = 0;
};

#endif  // CHROME_BROWSER_UI_GLOBAL_ERROR_GLOBAL_ERROR_BUBBLE_VIEW_BASE_H_
