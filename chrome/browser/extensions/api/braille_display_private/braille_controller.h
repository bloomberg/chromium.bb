// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BRAILLE_DISPLAY_PRIVATE_BRAILLE_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_API_BRAILLE_DISPLAY_PRIVATE_BRAILLE_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/common/extensions/api/braille_display_private.h"

namespace extensions {
namespace api {
namespace braille_display_private {
class BrailleObserver;

// Singleton class that controls the braille display.
class BrailleController {
 public:
  static BrailleController* GetInstance();

  virtual std::unique_ptr<DisplayState> GetDisplayState() = 0;
  virtual void WriteDots(const std::vector<uint8_t>& cells,
                         unsigned int cols,
                         unsigned int rows) = 0;
  virtual void AddObserver(BrailleObserver* observer) = 0;
  virtual void RemoveObserver(BrailleObserver* observer) = 0;

 protected:
  BrailleController();
  virtual ~BrailleController();

 private:
  DISALLOW_COPY_AND_ASSIGN(BrailleController);
};

// Observer for events from the BrailleController
class BrailleObserver {
 public:
  virtual void OnBrailleDisplayStateChanged(
      const DisplayState& display_state) {}
  virtual void OnBrailleKeyEvent(const KeyEvent& event) {}
};

}  // namespace braille_display_private
}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BRAILLE_DISPLAY_PRIVATE_BRAILLE_CONTROLLER_H_
