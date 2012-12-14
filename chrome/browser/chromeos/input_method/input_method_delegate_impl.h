// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_DELEGATE_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_DELEGATE_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/input_method/input_method_delegate.h"

namespace chromeos {
namespace input_method {

// Accesses the hardware keyboard layout and application locale from the
// BrowserProcess.
class InputMethodDelegateImpl : public InputMethodDelegate {
 public:
  InputMethodDelegateImpl();

  // InputMethodDelegate implementation.
  virtual std::string GetHardwareKeyboardLayout() const OVERRIDE;
  virtual std::string GetActiveLocale() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputMethodDelegateImpl);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_DELEGATE_IMPL_H_
