// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_IMAGE_BACKGROUND_H_
#define CHROME_BROWSER_CHROMEOS_IMAGE_BACKGROUND_H_

#include "base/basictypes.h"
#include "views/background.h"
#include "app/gfx/canvas.h"

namespace gfx {
class Canvas;
}

namespace views {

class ImageBackground : public Background {
  public:
    explicit ImageBackground(GdkPixbuf* background_image)
      : background_image_(background_image) {
    }

    virtual ~ImageBackground() {}

    void Paint(gfx::Canvas* canvas, View* view) const {
      canvas->DrawGdkPixbuf(background_image_, 0, 0);
    }
  private:
    // Background image that is drawn by this background.
    // This class does _not_ take ownership of the image.
    GdkPixbuf* background_image_;
    DISALLOW_COPY_AND_ASSIGN(ImageBackground);
};

}  // namespace views

#endif  // CHROME_BROWSER_CHROMEOS_IMAGE_BACKGROUND_H_

