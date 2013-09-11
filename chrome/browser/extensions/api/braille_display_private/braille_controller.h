// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BRAILLE_DISPLAY_PRIVATE_BRAILLE_CONTROLLER_H_

#define CHROME_BROWSER_EXTENSIONS_API_BRAILLE_DISPLAY_PRIVATE_BRAILLE_CONTROLLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/common/extensions/api/braille_display_private.h"

namespace extensions {
namespace api {
namespace braille_display_private {
class BrailleObserver;

#ifdef USE_BRLAPI
class BrlapiConnection;
#endif  // USE_BRLAPI

// Singleton class that controls the braille display.
class BrailleController {
 public:
  static BrailleController* GetInstance();

#ifdef USE_BRLAPI
  typedef base::Callback<scoped_ptr<BrlapiConnection>()>
      CreateBrlapiConnectionFunction;
  // For dependency injection in tests.  Sets the function used to create
  // brlapi connections.
  virtual void SetCreateBrlapiConnectionForTesting(
      const CreateBrlapiConnectionFunction& callback) = 0;
#endif  // USE_BRLAPI

  virtual scoped_ptr<base::DictionaryValue> GetDisplayState() = 0;
  virtual void WriteDots(const std::string& cells) = 0;
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
  virtual void OnKeyEvent(
      const extensions::api::braille_display_private::KeyEvent &event) {}
};

}  // namespace braille_display_private
}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BRAILLE_DISPLAY_PRIVATE_BRAILLE_CONTROLLER_H_
