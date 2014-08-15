// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_INPUT_METHOD_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_INPUT_METHOD_MANAGER_H_

#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chromeos/ime/component_extension_ime_manager.h"
#include "chromeos/ime/fake_ime_keyboard.h"
#include "chromeos/ime/fake_input_method_delegate.h"
#include "chromeos/ime/input_method_manager.h"
#include "chromeos/ime/input_method_whitelist.h"

namespace chromeos {
namespace input_method {

// The mock implementation of InputMethodManager for testing.
class MockInputMethodManager : public InputMethodManager {
 public:
  class State : public InputMethodManager::State {
   public:
    explicit State(MockInputMethodManager* manager);

    virtual scoped_refptr<InputMethodManager::State> Clone() const OVERRIDE;
    virtual void AddInputMethodExtension(
        const std::string& extension_id,
        const InputMethodDescriptors& descriptors,
        InputMethodEngineInterface* instance) OVERRIDE;
    virtual void RemoveInputMethodExtension(
        const std::string& extension_id) OVERRIDE;
    virtual void ChangeInputMethod(const std::string& input_method_id,
                                   bool show_message) OVERRIDE;
    virtual bool EnableInputMethod(
        const std::string& new_active_input_method_id) OVERRIDE;
    virtual void EnableLoginLayouts(
        const std::string& language_code,
        const std::vector<std::string>& initial_layouts) OVERRIDE;
    virtual void EnableLockScreenLayouts() OVERRIDE;
    virtual void GetInputMethodExtensions(
        InputMethodDescriptors* result) OVERRIDE;
    virtual scoped_ptr<InputMethodDescriptors> GetActiveInputMethods()
        const OVERRIDE;
    virtual const std::vector<std::string>& GetActiveInputMethodIds()
        const OVERRIDE;
    virtual const InputMethodDescriptor* GetInputMethodFromId(
        const std::string& input_method_id) const OVERRIDE;
    virtual size_t GetNumActiveInputMethods() const OVERRIDE;
    virtual void SetEnabledExtensionImes(
        std::vector<std::string>* ids) OVERRIDE;
    virtual void SetInputMethodLoginDefault() OVERRIDE;
    virtual void SetInputMethodLoginDefaultFromVPD(
        const std::string& locale,
        const std::string& layout) OVERRIDE;
    virtual bool SwitchToNextInputMethod() OVERRIDE;
    virtual bool SwitchToPreviousInputMethod(
        const ui::Accelerator& accelerator) OVERRIDE;
    virtual bool SwitchInputMethod(const ui::Accelerator& accelerator) OVERRIDE;
    virtual InputMethodDescriptor GetCurrentInputMethod() const OVERRIDE;
    virtual bool ReplaceEnabledInputMethods(
        const std::vector<std::string>& new_active_input_method_ids) OVERRIDE;

    // The value GetCurrentInputMethod().id() will return.
    std::string current_input_method_id;

    // The active input method ids cache (actually default only)
    std::vector<std::string> active_input_method_ids;

   protected:
    friend base::RefCounted<chromeos::input_method::InputMethodManager::State>;
    virtual ~State();

    MockInputMethodManager* const manager_;
  };

  MockInputMethodManager();
  virtual ~MockInputMethodManager();

  // InputMethodManager override:
  virtual UISessionState GetUISessionState() OVERRIDE;
  virtual void AddObserver(InputMethodManager::Observer* observer) OVERRIDE;
  virtual void AddCandidateWindowObserver(
      InputMethodManager::CandidateWindowObserver* observer) OVERRIDE;
  virtual void RemoveObserver(InputMethodManager::Observer* observer) OVERRIDE;
  virtual void RemoveCandidateWindowObserver(
      InputMethodManager::CandidateWindowObserver* observer) OVERRIDE;
  virtual scoped_ptr<InputMethodDescriptors>
      GetSupportedInputMethods() const OVERRIDE;
  virtual void ActivateInputMethodMenuItem(const std::string& key) OVERRIDE;
  virtual bool IsISOLevel5ShiftUsedByCurrentInputMethod() const OVERRIDE;
  virtual bool IsAltGrUsedByCurrentInputMethod() const OVERRIDE;
  virtual ImeKeyboard* GetImeKeyboard() OVERRIDE;
  virtual InputMethodUtil* GetInputMethodUtil() OVERRIDE;
  virtual ComponentExtensionIMEManager*
      GetComponentExtensionIMEManager() OVERRIDE;
  virtual bool IsLoginKeyboard(const std::string& layout) const OVERRIDE;
  virtual bool MigrateInputMethods(
       std::vector<std::string>* input_method_ids) OVERRIDE;
  virtual scoped_refptr<InputMethodManager::State> CreateNewState(
      Profile* profile) OVERRIDE;
  virtual scoped_refptr<InputMethodManager::State> GetActiveIMEState() OVERRIDE;
  virtual void SetState(
      scoped_refptr<InputMethodManager::State> state) OVERRIDE;

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
