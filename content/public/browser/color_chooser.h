// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_COLOR_CHOOSER_H_
#define CONTENT_PUBLIC_BROWSER_COLOR_CHOOSER_H_

#include "third_party/skia/include/core/SkColor.h"

namespace content {

class WebContents;

// Abstraction object for color choosers for each platform.
class ColorChooser {
 public:
  static ColorChooser* Create(int identifier,
                              WebContents* web_contents,
                              SkColor initial_color);
  ColorChooser(int identifier) : identifier_(identifier) {}
  virtual ~ColorChooser() {}

  // Returns a unique identifier for this chooser.  Identifiers are unique
  // across a renderer process.  This avoids race conditions in synchronizing
  // the browser and renderer processes.  For example, if a renderer closes one
  // chooser and opens another, and simultaneously the user picks a color in the
  // first chooser, the IDs can be used to drop the "chose a color" message
  // rather than erroneously tell the renderer that the user picked a color in
  // the second chooser.
  int identifier() const { return identifier_; }

  // Ends connection with color chooser. Closes color chooser depending on the
  // platform.
  virtual void End() = 0;

  // Sets the selected color.
  virtual void SetSelectedColor(SkColor color) = 0;

private:
  int identifier_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_COLOR_CHOOSER_H_
