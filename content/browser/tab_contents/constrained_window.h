// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TAB_CONTENTS_CONSTRAINED_WINDOW_H_
#define CONTENT_BROWSER_TAB_CONTENTS_CONSTRAINED_WINDOW_H_
#pragma once

#include "build/build_config.h"

///////////////////////////////////////////////////////////////////////////////
// ConstrainedWindow
//
//  This interface represents a window that is constrained to a TabContents'
//  bounds.
//
class ConstrainedWindow {
 public:
  // Makes the Constrained Window visible. Only one Constrained Window is shown
  // at a time per tab.
  virtual void ShowConstrainedWindow() = 0;

  // Closes the Constrained Window.
  virtual void CloseConstrainedWindow() = 0;

  // Sets focus on the Constrained Window.
  virtual void FocusConstrainedWindow() {}

 protected:
  virtual ~ConstrainedWindow() {}
};

#endif  // CONTENT_BROWSER_TAB_CONTENTS_CONSTRAINED_WINDOW_H_
