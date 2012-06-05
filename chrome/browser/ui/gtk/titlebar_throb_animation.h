// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_TITLEBAR_THROB_ANIMATION_H_
#define CHROME_BROWSER_UI_GTK_TITLEBAR_THROB_ANIMATION_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/gfx/gtk_util.h"

// A helper class to keep track of which frame of the throbber animation
// we're showing.
class TitlebarThrobAnimation {
 public:
  TitlebarThrobAnimation();

  // Get the next frame in the animation. The image is owned by the throbber
  // so the caller doesn't need to unref. |is_waiting| is true if we're
  // still waiting for a response.
  GdkPixbuf* GetNextFrame(bool is_waiting);

  // Reset back to the first frame.
  void Reset();

 private:
  // Make sure the frames are loaded.
  static void InitFrames();

  int current_frame_;
  int current_waiting_frame_;

  DISALLOW_COPY_AND_ASSIGN(TitlebarThrobAnimation);
};

#endif  // CHROME_BROWSER_UI_GTK_TITLEBAR_THROB_ANIMATION_H_
