// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BUBBLE_POSITIONER_H_
#define CHROME_BROWSER_BUBBLE_POSITIONER_H_

namespace gfx {
class Rect;
}

// An object in the browser UI can implement this interface to provide display
// bounds for the omnibox bubble and info bubble views.
class BubblePositioner {
 public:
  // Returns the bounds of the "location bar" stack (including star/go buttons
  // where relevant).  The omnibox dropdown uses this to calculate its width and
  // y-coordinate, and views showing InfoBubbles use it to find the y-coordinate
  // they should show at, so that all "bubble" UIs show up at the same vertical
  // position.
  virtual gfx::Rect GetLocationStackBounds() const = 0;
};

#endif  // CHROME_BROWSER_BUBBLE_POSITIONER_H_
