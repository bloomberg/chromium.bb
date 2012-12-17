// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_DELEGATE_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace chromeos {
namespace input_method {

// Provides access to read/persist Input Method-related properties.
class InputMethodDelegate {
 public:
  InputMethodDelegate() {}
  virtual ~InputMethodDelegate() {}

  // Retrieves the hardware keyboard layout ID. May return an empty string if
  // the ID is unknown.
  virtual std::string GetHardwareKeyboardLayout() const = 0;
  // Retrieves the currently active UI locale.
  virtual std::string GetActiveLocale() const = 0;
  // Retrieves the default task runner. The public input_method API should only
  // be accessed in the context of this task runner.
  virtual scoped_refptr<base::SequencedTaskRunner>
      GetDefaultTaskRunner() const = 0;
  // Retrieves a task runner that may be used for possibly blocking tasks such
  // as file IO.
  virtual scoped_refptr<base::SequencedTaskRunner>
      GetWorkerTaskRunner() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputMethodDelegate);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_DELEGATE_H_
