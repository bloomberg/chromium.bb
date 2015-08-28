// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BUBBLE_BUBBLE_UI_H_
#define COMPONENTS_BUBBLE_BUBBLE_UI_H_

class BubbleUI {
 public:
  virtual ~BubbleUI() {}

  // Should display the bubble UI.
  virtual void Show() = 0;

  // Should close the bubble UI.
  virtual void Close() = 0;

  // Should update the bubble UI's position.
  // Important to verify that an anchor is still available.
  // ex: fullscreen might not have a location bar in views.
  virtual void UpdateAnchorPosition() = 0;
};

#endif  // COMPONENTS_BUBBLE_BUBBLE_UI_H_
