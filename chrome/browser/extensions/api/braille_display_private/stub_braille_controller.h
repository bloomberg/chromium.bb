// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BRAILLE_DISPLAY_PRIVATE_STUB_BRAILLE_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_API_BRAILLE_DISPLAY_PRIVATE_STUB_BRAILLE_CONTROLLER_H_

#include "chrome/browser/extensions/api/braille_display_private/braille_controller.h"

namespace extensions {
namespace api {
namespace braille_display_private {

// Stub implementation for the BrailleController interface.
class StubBrailleController : public BrailleController {
 public:
  StubBrailleController();
  virtual scoped_ptr<DisplayState> GetDisplayState() OVERRIDE;
  virtual void WriteDots(const std::string& cells) OVERRIDE;
  virtual void AddObserver(BrailleObserver* observer) OVERRIDE;
  virtual void RemoveObserver(BrailleObserver* observer) OVERRIDE;
};

}  // namespace braille_display_private
}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BRAILLE_DISPLAY_PRIVATE_STUB_BRAILLE_CONTROLLER_H_
