// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_MANAGER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_MANAGER_IMPL_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/chromeos/input_method/candidate_window_controller.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ime/input_method_manager.h"
#include "chromeos/ime/input_method_whitelist.h"

namespace chromeos {
class ComponentExtensionIMEManager;
class ComponentExtensionIMEManagerDelegate;
class InputMethodEngine;
namespace input_method {
class InputMethodDelegate;
class ImeKeyboard;

// The implementation of InputMethodManager.
class InputMethodManagerImpl : public InputMethodManager,
                               public CandidateWindowController::Observer {
 public:
  // Constructs an InputMethodManager instance. The client is responsible for
  // calling |SetState| in response to relevant changes in browser state.
  explicit InputMethodManagerImpl(scoped_ptr<InputMethodDelegate> delegate);
  virtual ~InputMethodManagerImpl();

  // Receives notification of an InputMethodManager::State transition.
  void SetState(State new_state);

  // InputMethodManager override:
  virtual void InitializeComponentExtension() OVERRIDE;
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
  virtual const std::vector<std::string>& GetActiveInputMethodIds() const
      OVERRIDE;
  virtual size_t GetNumActiveInputMethods() const OVERRIDE;
  virtual const InputMethodDescriptor* GetInputMethodFromId(
      const std::string& input_method_id) const OVERRIDE;
  virtual void EnableLoginLayouts(
      const std::string& language_code,
      const std::vector<std::string>& initial_layouts) OVERRIDE;
  virtual bool ReplaceEnabledInputMethods(
      const std::vector<std::string>& new_active_input_method_ids) OVERRIDE;
  virtual bool EnableInputMethod(const std::string& new_active_input_method_id)
      OVERRIDE;
  virtual void ChangeInputMethod(const std::string& input_method_id) OVERRIDE;
  virtual void ActivateInputMethodMenuItem(const std::string& key) OVERRIDE;
  virtual void AddInputMethodExtension(
      const std::string& extension_id,
      const InputMethodDescriptors& descriptors,
      InputMethodEngineInterface* instance) OVERRIDE;
  virtual void RemoveInputMethodExtension(
      const std::string& extension_id) OVERRIDE;
  virtual void GetInputMethodExtensions(
      InputMethodDescriptors* result) OVERRIDE;
  virtual void SetEnabledExtensionImes(std::vector<std::string>* ids) OVERRIDE;
  virtual void SetInputMethodLoginDefault() OVERRIDE;
  virtual void SetInputMethodLoginDefaultFromVPD(
      const std::string& locale, const std::string& layout) OVERRIDE;
  virtual bool SwitchToNextInputMethod() OVERRIDE;
  virtual bool SwitchToPreviousInputMethod(
      const ui::Accelerator& accelerator) OVERRIDE;
  virtual bool SwitchInputMethod(const ui::Accelerator& accelerator) OVERRIDE;
  virtual InputMethodDescriptor GetCurrentInputMethod() const OVERRIDE;
  virtual bool IsISOLevel5ShiftUsedByCurrentInputMethod() const OVERRIDE;
  virtual bool IsAltGrUsedByCurrentInputMethod() const OVERRIDE;

  virtual ImeKeyboard* GetImeKeyboard() OVERRIDE;
  virtual InputMethodUtil* GetInputMethodUtil() OVERRIDE;
  virtual ComponentExtensionIMEManager*
      GetComponentExtensionIMEManager() OVERRIDE;
  virtual bool IsLoginKeyboard(const std::string& layout) const OVERRIDE;

  virtual bool MigrateInputMethods(
      std::vector<std::string>* input_method_ids) OVERRIDE;

  // Sets |candidate_window_controller_|.
  void SetCandidateWindowControllerForTesting(
      CandidateWindowController* candidate_window_controller);
  // Sets |keyboard_|.
  void SetImeKeyboardForTesting(ImeKeyboard* keyboard);
  // Initialize |component_extension_manager_|.
  void InitializeComponentExtensionForTesting(
      scoped_ptr<ComponentExtensionIMEManagerDelegate> delegate);

 private:
  friend class InputMethodManagerImplTest;

  // CandidateWindowController::Observer overrides:
  virtual void CandidateClicked(int index) OVERRIDE;
  virtual void CandidateWindowOpened() OVERRIDE;
  virtual void CandidateWindowClosed() OVERRIDE;

  // Temporarily deactivates all input methods (e.g. Chinese, Japanese, Arabic)
  // since they are not necessary to input a login password. Users are still
  // able to use/switch active keyboard layouts (e.g. US qwerty, US dvorak,
  // French).
  void OnScreenLocked();

  // Resumes the original state by activating input methods and/or changing the
  // current input method as needed.
  void OnScreenUnlocked();

  // Returns true if |input_method_id| is in |active_input_method_ids_|.
  bool InputMethodIsActivated(const std::string& input_method_id);

  // Returns true if the given input method config value is a string list
  // that only contains an input method ID of a keyboard layout.
  bool ContainsOnlyKeyboardLayout(const std::vector<std::string>& value);

  // Creates and initializes |candidate_window_controller_| if it hasn't been
  // done.
  void MaybeInitializeCandidateWindowController();

  // If |current_input_method_id_| is not in |input_method_ids|, switch to
  // input_method_ids[0]. If the ID is equal to input_method_ids[N], switch to
  // input_method_ids[N+1].
  void SwitchToNextInputMethodInternal(
      const std::vector<std::string>& input_method_ids,
      const std::string& current_input_method_id);

  // Change system input method.
  // Returns true if the system input method is changed.
  bool ChangeInputMethodInternal(const std::string& input_method_id,
                                 bool show_message);

  // Loads necessary component extensions.
  // TODO(nona): Support dynamical unloading.
  void LoadNecessaryComponentExtensions();

  // Adds new input method to given list if possible
  bool EnableInputMethodImpl(
      const std::string& input_method_id,
      std::vector<std::string>* new_active_input_method_ids) const;

  // Starts or stops the system input method framework as needed.
  // (after list of enabled input methods has been updated)
  void ReconfigureIMFramework();

  scoped_ptr<InputMethodDelegate> delegate_;

  // The current browser status.
  State state_;

  // A list of objects that monitor the manager.
  ObserverList<InputMethodManager::Observer> observers_;
  ObserverList<CandidateWindowObserver> candidate_window_observers_;

  // The input method which was/is selected.
  InputMethodDescriptor previous_input_method_;
  InputMethodDescriptor current_input_method_;
  // The active input method ids cache.
  std::vector<std::string> active_input_method_ids_;

  // The list of enabled extension IMEs.
  std::vector<std::string> enabled_extension_imes_;

  // For screen locker. When the screen is locked, |previous_input_method_|,
  // |current_input_method_|, and |active_input_method_ids_| above are copied
  // to these "saved" variables.
  InputMethodDescriptor saved_previous_input_method_;
  InputMethodDescriptor saved_current_input_method_;
  std::vector<std::string> saved_active_input_method_ids_;

  // Extra input methods that have been explicitly added to the menu, such as
  // those created by extension.
  std::map<std::string, InputMethodDescriptor> extra_input_methods_;

  // The candidate window.  This will be deleted when the APP_TERMINATING
  // message is sent.
  scoped_ptr<CandidateWindowController> candidate_window_controller_;

  // An object which provides miscellaneous input method utility functions. Note
  // that |util_| is required to initialize |keyboard_|.
  InputMethodUtil util_;

  // An object which provides component extension ime management functions.
  scoped_ptr<ComponentExtensionIMEManager> component_extension_ime_manager_;

  // An object for switching XKB layouts and keyboard status like caps lock and
  // auto-repeat interval.
  scoped_ptr<ImeKeyboard> keyboard_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<InputMethodManagerImpl> weak_ptr_factory_;

  // The engine map from extension_id to an engine.
  std::map<std::string, InputMethodEngineInterface*> engine_map_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodManagerImpl);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_MANAGER_IMPL_H_
