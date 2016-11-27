// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_MANAGER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_MANAGER_IMPL_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/chromeos/input_method/candidate_window_controller.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/chromeos/input_method_whitelist.h"
#include "ui/base/ime/ime_engine_handler_interface.h"

namespace ui {
class IMEEngineHandlerInterface;
}  // namespace ui

namespace chromeos {
class ComponentExtensionIMEManager;
class ComponentExtensionIMEManagerDelegate;
namespace input_method {
class InputMethodDelegate;
class ImeKeyboard;

// The implementation of InputMethodManager.
class InputMethodManagerImpl : public InputMethodManager,
                               public CandidateWindowController::Observer,
                               public UserAddingScreen::Observer {
 public:
  class StateImpl : public InputMethodManager::State {
   public:
    StateImpl(InputMethodManagerImpl* manager, Profile* profile);

    // Init new state as a copy of other.
    void InitFrom(const StateImpl& other);

    // Returns true if (manager_->state_ == this).
    bool IsActive() const;

    // Returns human-readable dump (for debug).
    std::string Dump() const;

    // Adds new input method to given list if possible
    bool EnableInputMethodImpl(
        const std::string& input_method_id,
        std::vector<std::string>* new_active_input_method_ids) const;

    // Returns true if |input_method_id| is in |active_input_method_ids|.
    bool InputMethodIsActivated(const std::string& input_method_id) const;

    // If |current_input_methodid_| is not in |input_method_ids|, switch to
    // input_method_ids[0]. If the ID is equal to input_method_ids[N], switch to
    // input_method_ids[N+1].
    void SwitchToNextInputMethodInternal(
        const std::vector<std::string>& input_method_ids,
        const std::string& current_input_methodid);

    // Returns the IDs of the subset of input methods which are active and are
    // associated with |accelerator|. For example,
    // { "mozc-hangul", "xkb:kr:kr104:kor" } is returned for
    // ui::VKEY_DBE_SBCSCHAR if the two input methods are active.
    void GetCandidateInputMethodsForAccelerator(
        const ui::Accelerator& accelerator,
        std::vector<std::string>* out_candidate_ids);

    // Returns true if given input method requires pending extension.
    bool MethodAwaitsExtensionLoad(const std::string& input_method_id) const;

    // InputMethodManager::State overrides.
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
    std::unique_ptr<InputMethodDescriptors> GetActiveInputMethods()
        const override;
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

    // ------------------------- Data members.
    Profile* const profile;

    // The input method which was/is selected.
    InputMethodDescriptor previous_input_method;
    InputMethodDescriptor current_input_method;

    // The active input method ids cache.
    std::vector<std::string> active_input_method_ids;

    // The pending input method id for delayed 3rd party IME enabling.
    std::string pending_input_method_id;

    // The list of enabled extension IMEs.
    std::vector<std::string> enabled_extension_imes;

    // Extra input methods that have been explicitly added to the menu, such as
    // those created by extension.
    std::map<std::string, InputMethodDescriptor> extra_input_methods;

    InputMethodManagerImpl* const manager_;

    // True if the opt-in IME menu is activated.
    bool menu_activated;

   protected:
    friend base::RefCounted<chromeos::input_method::InputMethodManager::State>;
    ~StateImpl() override;
  };

  // Constructs an InputMethodManager instance. The client is responsible for
  // calling |SetUISessionState| in response to relevant changes in browser
  // state.
  InputMethodManagerImpl(std::unique_ptr<InputMethodDelegate> delegate,
                         bool enable_extension_loading);
  ~InputMethodManagerImpl() override;

  // Receives notification of an InputMethodManager::UISessionState transition.
  void SetUISessionState(UISessionState new_ui_session);

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
  std::unique_ptr<InputMethodDescriptors> GetSupportedInputMethods()
      const override;
  void ActivateInputMethodMenuItem(const std::string& key) override;
  bool IsISOLevel5ShiftUsedByCurrentInputMethod() const override;
  bool IsAltGrUsedByCurrentInputMethod() const override;
  void NotifyImeMenuItemsChanged(
      const std::string& engine_id,
      const std::vector<InputMethodManager::MenuItem>& items) override;
  void MaybeNotifyImeMenuActivationChanged() override;
  void OverrideKeyboardUrlRef(const std::string& keyset) override;
  bool IsEmojiHandwritingVoiceOnImeMenuEnabled() override;

  // chromeos::UserAddingScreen:
  void OnUserAddingStarted() override;
  void OnUserAddingFinished() override;

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

  // Sets |candidate_window_controller_|.
  void SetCandidateWindowControllerForTesting(
      CandidateWindowController* candidate_window_controller);
  // Sets |keyboard_|.
  void SetImeKeyboardForTesting(ImeKeyboard* keyboard);
  // Initialize |component_extension_manager_|.
  void InitializeComponentExtensionForTesting(
      std::unique_ptr<ComponentExtensionIMEManagerDelegate> delegate);

 private:
  friend class InputMethodManagerImplTest;

  // CandidateWindowController::Observer overrides:
  void CandidateClicked(int index) override;
  void CandidateWindowOpened() override;
  void CandidateWindowClosed() override;

  // Temporarily deactivates all input methods (e.g. Chinese, Japanese, Arabic)
  // since they are not necessary to input a login password. Users are still
  // able to use/switch active keyboard layouts (e.g. US qwerty, US dvorak,
  // French).
  void OnScreenLocked();

  // Resumes the original state by activating input methods and/or changing the
  // current input method as needed.
  void OnScreenUnlocked();

  // Returns true if the given input method config value is a string list
  // that only contains an input method ID of a keyboard layout.
  bool ContainsOnlyKeyboardLayout(const std::vector<std::string>& value);

  // Creates and initializes |candidate_window_controller_| if it hasn't been
  // done.
  void MaybeInitializeCandidateWindowController();

  // Returns Input Method that best matches given id.
  const InputMethodDescriptor* LookupInputMethod(
      const std::string& input_method_id,
      StateImpl* state);

  // Change system input method.
  void ChangeInputMethodInternal(const InputMethodDescriptor& descriptor,
                                 Profile* profile,
                                 bool show_message,
                                 bool notify_menu);

  // Loads necessary component extensions.
  // TODO(nona): Support dynamical unloading.
  void LoadNecessaryComponentExtensions(StateImpl* state);

  // Starts or stops the system input method framework as needed.
  // (after list of enabled input methods has been updated).
  // If state is active, active input method is updated.
  void ReconfigureIMFramework(StateImpl* state);

  // Record input method usage histograms.
  void RecordInputMethodUsage(const std::string& input_method_id);

  // Notifies the current input method or the list of active input method IDs
  // changed.
  void NotifyImeMenuListChanged();

  std::unique_ptr<InputMethodDelegate> delegate_;

  // The current UI session status.
  UISessionState ui_session_;

  // A list of objects that monitor the manager.
  base::ObserverList<InputMethodManager::Observer> observers_;
  base::ObserverList<CandidateWindowObserver> candidate_window_observers_;
  base::ObserverList<ImeMenuObserver> ime_menu_observers_;

  scoped_refptr<StateImpl> state_;

  // The candidate window.  This will be deleted when the APP_TERMINATING
  // message is sent.
  std::unique_ptr<CandidateWindowController> candidate_window_controller_;

  // An object which provides miscellaneous input method utility functions. Note
  // that |util_| is required to initialize |keyboard_|.
  InputMethodUtil util_;

  // An object which provides component extension ime management functions.
  std::unique_ptr<ComponentExtensionIMEManager>
      component_extension_ime_manager_;

  // An object for switching XKB layouts and keyboard status like caps lock and
  // auto-repeat interval.
  std::unique_ptr<ImeKeyboard> keyboard_;

  // Whether load IME extensions.
  bool enable_extension_loading_;

  // Whether the expanded IME menu is activated.
  bool is_ime_menu_activated_;

  // The engine map from extension_id to an engine.
  typedef std::map<std::string, ui::IMEEngineHandlerInterface*> EngineMap;
  typedef std::map<Profile*, EngineMap, ProfileCompare> ProfileEngineMap;
  ProfileEngineMap engine_map_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodManagerImpl);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_MANAGER_IMPL_H_
