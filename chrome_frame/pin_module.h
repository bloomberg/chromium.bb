// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_PIN_MODULE_H_
#define CHROME_FRAME_PIN_MODULE_H_

namespace chrome_frame {

typedef void (*PinModuleCallbackFn)(void);

// Sets a callback function to be invoked when the module is pinned.
void SetPinModuleCallback(PinModuleCallbackFn callback);

// Utility function that prevents the current module from ever being unloaded.
// Call if you make irreversible patches.
void PinModule();

}  // namespace chrome_frame

#endif  // CHROME_FRAME_PIN_MODULE_H_
