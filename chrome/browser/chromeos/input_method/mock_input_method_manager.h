// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_INPUT_METHOD_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_INPUT_METHOD_MANAGER_H_

#include <stddef.h>

#include "base/macros.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "ui/base/ime/chromeos/component_extension_ime_manager.h"
#include "ui/base/ime/chromeos/fake_ime_keyboard.h"
#include "ui/base/ime/chromeos/fake_input_method_delegate.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/chromeos/input_method_whitelist.h"

namespace chromeos {
namespace input_method {

// The mock implementation of InputMethodManager for testing.
class MockInputMethodManager : public InputMethodManager {
 public:
  class State : public InputMethodManager::State {
   public:
    explicit State(MockInputMethodManager* manager);

    scoped_refptr<InputMethodManager::State> Clone() const override;
    void AddInputMethodExtension(
        const std::string& extension_id,
        const InputMethodDescriptors& descriptors,
        ui::IMEEngineHandlerInterface* instance) override;
    void RemoveInputMethodExtension(const std::string& extension_id) override;
    void ChangeInputMethod(const std::string& input_method_id,
                           bool show_message) override;
    bool EnableInputMethod(
        const std::string& new_active_input_method_id) override;
    void EnableLoginLayouts(
        const std::string& language_code,
        const std::vector<std::string>& initial_layouts) override;
    void EnableLockScreenLayouts() override;
    void GetInputMethodExtensions(InputMethodDescriptors* result) override;
    scoped_ptr<InputMethodDescriptors> GetActiveInputMethods() const override;
    const std::vector<std::string>& GetActiveInputMethodIds() const override;
    const InputMethodDescriptor* GetInputMethodFromId(
        const std::string& input_method_id) const override;
    size_t GetNumActiveInputMethods() const override;
    void SetEnabledExtensionImes(std::vector<std::string>* ids) override;
    void SetInputMethodLoginDefault() override;
    void SetInputMethodLoginDefaultFromVPD(const std::string& locale,
                                           const std::string& layout) override;
    bool CanCycleInputMethod() override;
    void SwitchToNextInputMethod() override;
    void SwitchToPreviousInputMethod() override;
    bool CanSwitchInputMethod(const ui::Accelerator& accelerator) override;
    void SwitchInputMethod(const ui::Accelerator& accelerator) override;
    InputMethodDescriptor GetCurrentInputMethod() const override;
    bool ReplaceEnabledInputMethods(
        const std::vector<std::string>& new_active_input_method_ids) override;

    // The value GetCurrentInputMethod().id() will return.
    std::string current_input_method_id;

    // The active input method ids cache (actually default only)
    std::vector<std::string> active_input_method_ids;

   protected:
    friend base::RefCounted<chromeos::input_method::InputMethodManager::State>;
    ~State() override;

    MockInputMethodManager* const manager_;
  };

  MockInputMethodManager();
  ~MockInputMethodManager() override;

  // InputMethodManager override:
  UISessionState GetUISessionState() override;
  void AddObserver(InputMethodManager::Observer* observer) override;
  void AddCandidateWindowObserver(
      InputMethodManager::CandidateWindowObserver* observer) override;
  void AddImeMenuObserver(
      InputMethodManager::ImeMenuObserver* observer) override;
  void RemoveObserver(InputMethodManager::Observer* observer) override;
  void RemoveCandidateWindowObserver(
      InputMethodManager::CandidateWindowObserver* observer) override;
  void RemoveImeMenuObserver(
      InputMethodManager::ImeMenuObserver* observer) override;
  scoped_ptr<InputMethodDescriptors> GetSupportedInputMethods() const override;
  void ActivateInputMethodMenuItem(const std::string& key) override;
  bool IsISOLevel5ShiftUsedByCurrentInputMethod() const override;
  bool IsAltGrUsedByCurrentInputMethod() const override;
  ImeKeyboard* GetImeKeyboard() override;
  InputMethodUtil* GetInputMethodUtil() override;
  ComponentExtensionIMEManager* GetComponentExtensionIMEManager() override;
  bool IsLoginKeyboard(const std::string& layout) const override;
  bool MigrateInputMethods(std::vector<std::string>* input_method_ids) override;
  scoped_refptr<InputMethodManager::State> CreateNewState(
      Profile* profile) override;
  scoped_refptr<InputMethodManager::State> GetActiveIMEState() override;
  void SetState(scoped_refptr<InputMethodManager::State> state) override;
  void ImeMenuActivationChanged(bool is_active) override;

  // Sets an input method ID which will be returned by GetCurrentInputMethod().
  void SetCurrentInputMethodId(const std::string& input_method_id);

  void SetComponentExtensionIMEManager(
      scoped_ptr<ComponentExtensionIMEManager> comp_ime_manager);

  // Set values that will be provided to the InputMethodUtil.
  void set_application_locale(const std::string& value);

  // Set the value returned by IsISOLevel5ShiftUsedByCurrentInputMethod
  void set_mod3_used(bool value) { mod3_used_ = value; }

  // TODO(yusukes): Add more variables for counting the numbers of the API calls
  int add_observer_count_;
  int remove_observer_count_;

 protected:
  scoped_refptr<State> state_;

 private:
  FakeInputMethodDelegate delegate_;  // used by util_
  InputMethodUtil util_;
  FakeImeKeyboard keyboard_;
  bool mod3_used_;
  scoped_ptr<ComponentExtensionIMEManager> comp_ime_manager_;

  DISALLOW_COPY_AND_ASSIGN(MockInputMethodManager);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_INPUT_METHOD_MANAGER_H_
