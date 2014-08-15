// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_PERSISTENCE_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_PERSISTENCE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chromeos/ime/input_method_manager.h"

namespace chromeos {
namespace input_method {

// Observes input method and session state changes, and persists input method
// changes to the BrowserProcess local state or to the user preferences,
// according to the session state.
class InputMethodPersistence : public InputMethodManager::Observer {
 public:
  // Constructs an instance that will observe input method changes on the
  // provided InputMethodManager. The client is responsible for calling
  // OnSessionStateChange whenever the InputMethodManager::UISessionState
  // changes.
  explicit InputMethodPersistence(InputMethodManager* input_method_manager);
  virtual ~InputMethodPersistence();

  // Receives notification of session state changes.
  void OnSessionStateChange(InputMethodManager::UISessionState new_session);

  // InputMethodManager::Observer overrides.
  virtual void InputMethodChanged(InputMethodManager* manager,
                                  bool show_message) OVERRIDE;

 private:
  InputMethodManager* input_method_manager_;
  InputMethodManager::UISessionState ui_session_;
  DISALLOW_COPY_AND_ASSIGN(InputMethodPersistence);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_PERSISTENCE_H_
