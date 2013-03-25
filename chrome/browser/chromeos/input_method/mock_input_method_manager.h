// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_INPUT_METHOD_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_INPUT_METHOD_MANAGER_H_

#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/input_method/input_method_whitelist.h"
#include "chrome/browser/chromeos/input_method/mock_xkeyboard.h"
#include "chromeos/ime/mock_input_method_delegate.h"

namespace chromeos {
namespace input_method {

// The mock implementation of InputMethodManager for testing.
class MockInputMethodManager : public InputMethodManager {
 public:
  MockInputMethodManager();
  virtual ~MockInputMethodManager();

  // InputMethodManager override:
  virtual void AddObserver(InputMethodManager::Observer* observer) OVERRIDE;
  virtual void AddCandidateWindowObserver(
      InputMethodManager::CandidateWindowObserver* observer) OVERRIDE;
  virtual void RemoveObserver(InputMethodManager::Observer* observer) OVERRIDE;
  virtual void RemoveCandidateWindowObserver(
      InputMethodManager::CandidateWindowObserver* observer) OVERRIDE;
  virtual scoped_ptr<InputMethodDescriptors>
      GetSupportedInputMethods() const OVERRIDE;
  virtual scoped_ptr<InputMethodDescriptors>
      GetActiveInputMethods() const OVERRIDE;
  virtual size_t GetNumActiveInputMethods() const OVERRIDE;
  virtual void EnableLayouts(const std::string& language_code,
                             const std::string& initial_layout) OVERRIDE;
  virtual bool EnableInputMethods(
      const std::vector<std::string>& new_active_input_method_ids) OVERRIDE;
  virtual bool SetInputMethodConfig(
      const std::string& section,
      const std::string& config_name,
      const InputMethodConfigValue& value) OVERRIDE;
  virtual void ChangeInputMethod(const std::string& input_method_id) OVERRIDE;
  virtual void ActivateInputMethodProperty(const std::string& key) OVERRIDE;
  virtual void AddInputMethodExtension(
      const std::string& id,
      const std::string& name,
      const std::vector<std::string>& layouts,
      const std::string& language,
      InputMethodEngine* instance) OVERRIDE;
  virtual void RemoveInputMethodExtension(const std::string& id) OVERRIDE;
  virtual void GetInputMethodExtensions(
      InputMethodDescriptors* result) OVERRIDE;
  virtual void SetFilteredExtensionImes(std::vector<std::string>* ids) OVERRIDE;
  virtual bool SwitchToNextInputMethod() OVERRIDE;
  virtual bool SwitchToPreviousInputMethod() OVERRIDE;
  virtual bool SwitchInputMethod(const ui::Accelerator& accelerator) OVERRIDE;
  virtual InputMethodDescriptor GetCurrentInputMethod() const OVERRIDE;
  virtual InputMethodPropertyList
      GetCurrentInputMethodProperties() const OVERRIDE;
  virtual XKeyboard* GetXKeyboard() OVERRIDE;
  virtual InputMethodUtil* GetInputMethodUtil() OVERRIDE;

  // Sets an input method ID which will be returned by GetCurrentInputMethod().
  void SetCurrentInputMethodId(const std::string& input_method_id) {
    current_input_method_id_ = input_method_id;
  }

  // Set values that will be provided to the InputMethodUtil.
  void set_application_locale(const std::string& value);
  void set_hardware_keyboard_layout(const std::string& value);

  // TODO(yusukes): Add more variables for counting the numbers of the API calls
  int add_observer_count_;
  int remove_observer_count_;

 private:
  // The value GetCurrentInputMethod().id() will return.
  std::string current_input_method_id_;

  InputMethodWhitelist whitelist_;
  MockInputMethodDelegate delegate_;  // used by util_
  InputMethodUtil util_;
  MockXKeyboard xkeyboard_;

  DISALLOW_COPY_AND_ASSIGN(MockInputMethodManager);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_INPUT_METHOD_MANAGER_H_
