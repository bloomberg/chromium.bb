// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_FAKE_INPUT_METHOD_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_FAKE_INPUT_METHOD_LIBRARY_H_

#include "chrome/browser/chromeos/cros/input_method_library.h"
#include "third_party/cros/chromeos_input_method.h"

namespace chromeos {

class FakeInputMethodLibrary : public InputMethodLibrary {
 public:
  FakeInputMethodLibrary();
  virtual ~FakeInputMethodLibrary() {}
  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);
  virtual InputMethodDescriptors* GetActiveInputMethods();
  virtual size_t GetNumActiveInputMethods();
  virtual InputMethodDescriptors* GetSupportedInputMethods();
  virtual void ChangeInputMethod(const std::string& input_method_id);
  virtual void SetImePropertyActivated(const std::string& key,
                                       bool activated);
  virtual bool InputMethodIsActivated(const std::string& input_method_id);
  virtual bool GetImeConfig(
      const char* section, const char* config_name, ImeConfigValue* out_value);
  virtual bool SetImeConfig(const char* section,
                            const char* config_name,
                            const ImeConfigValue& value);

  virtual const InputMethodDescriptor& previous_input_method() const {
    return previous_input_method_;
  }
  virtual const InputMethodDescriptor& current_input_method() const {
    return current_input_method_;
  }

  virtual const ImePropertyList& current_ime_properties() const {
    return current_ime_properties_;
  }

 private:
  InputMethodDescriptor previous_input_method_;
  InputMethodDescriptor current_input_method_;
  ImePropertyList current_ime_properties_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_FAKE_INPUT_METHOD_LIBRARY_H_
