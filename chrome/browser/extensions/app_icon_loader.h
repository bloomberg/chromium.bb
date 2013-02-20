// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_APP_ICON_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_APP_ICON_LOADER_H_

#include <string>

#include "base/basictypes.h"

namespace gfx {
class ImageSkia;
}

namespace extensions {

// Interface used to load app icons. This is in its own class so that it can
// be mocked.
class AppIconLoader {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when the image for an app is loaded.
    virtual void SetAppImage(const std::string& id,
                             const gfx::ImageSkia& image) = 0;
  };

  AppIconLoader() {}
  virtual ~AppIconLoader() {}

  // Fetches the image for the specified id. When done (which may be
  // synchronous), this should invoke SetAppImage() on the delegate.
  virtual void FetchImage(const std::string& id) = 0;

  // Clears the image for the specified id.
  virtual void ClearImage(const std::string& id) = 0;

  // Updates the image for the specified id. This is called to re-create
  // the app icon with latest app state (enabled or disabled/terminiated).
  // SetAppImage() is called when done.
  virtual void UpdateImage(const std::string& id) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppIconLoader);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_APP_ICON_LOADER_H_
