// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_IME_INPUT_METHOD_MANAGER_H_
#define CHROMEOS_IME_INPUT_METHOD_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/ime/input_method_descriptor.h"

namespace ui {
class Accelerator;
}  // namespace ui

namespace chromeos {
class ComponentExtensionIMEManager;
class InputMethodEngineInterface;
namespace input_method {
class InputMethodUtil;
class ImeKeyboard;

// This class manages input methodshandles.  Classes can add themselves as
// observers. Clients can get an instance of this library class by:
// InputMethodManager::Get().
class CHROMEOS_EXPORT InputMethodManager {
 public:
  enum State {
    STATE_LOGIN_SCREEN = 0,
    STATE_BROWSER_SCREEN,
    STATE_LOCK_SCREEN,
    STATE_TERMINATING,
  };

  class Observer {
   public:
    virtual ~Observer() {}
    // Called when the current input method is changed.  |show_message|
    // indicates whether the user should be notified of this change.
    virtual void InputMethodChanged(InputMethodManager* manager,
                                    bool show_message) = 0;
  };

  // CandidateWindowObserver is notified of events related to the candidate
  // window.  The "suggestion window" used by IMEs such as ibus-mozc does not
  // count as the candidate window (this may change if we later want suggestion
  // window events as well).  These events also won't occur when the virtual
  // keyboard is used, since it controls its own candidate window.
  class CandidateWindowObserver {
   public:
    virtual ~CandidateWindowObserver() {}
    // Called when the candidate window is opened.
    virtual void CandidateWindowOpened(InputMethodManager* manager) = 0;
    // Called when the candidate window is closed.
    virtual void CandidateWindowClosed(InputMethodManager* manager) = 0;
  };

  virtual ~InputMethodManager() {}

  // Gets the global instance of InputMethodManager. Initialize() must be called
  // first.
  static CHROMEOS_EXPORT InputMethodManager* Get();

  // Sets the global instance. |instance| will be owned by the internal pointer
  // and deleted by Shutdown().
  // TODO(nona): Instanciate InputMethodManagerImpl inside of this function once
  //             crbug.com/164375 is fixed.
  static CHROMEOS_EXPORT void Initialize(InputMethodManager* instance);

  // Destroy the global instance.
  static CHROMEOS_EXPORT void Shutdown();

  // Adds an observer to receive notifications of input method related
  // changes as desribed in the Observer class above.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void AddCandidateWindowObserver(
      CandidateWindowObserver* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  virtual void RemoveCandidateWindowObserver(
      CandidateWindowObserver* observer) = 0;

  // Returns all input methods that are supported, including ones not active.
  // This function never returns NULL. Note that input method extensions are NOT
  // included in the result.
  virtual scoped_ptr<InputMethodDescriptors>
      GetSupportedInputMethods() const = 0;

  // Returns the list of input methods we can select (i.e. active) including
  // extension input methods.
  virtual scoped_ptr<InputMethodDescriptors> GetActiveInputMethods() const = 0;

  // Returns the list of input methods we can select (i.e. active) including
  // extension input methods.
  // The same as GetActiveInputMethods but returns reference to internal list.
  virtual const std::vector<std::string>& GetActiveInputMethodIds() const = 0;

  // Returns the number of active input methods including extension input
  // methods.
  virtual size_t GetNumActiveInputMethods() const = 0;

  // Returns the input method descriptor from the given input method id string.
  // If the given input method id is invalid, returns NULL.
  virtual const InputMethodDescriptor* GetInputMethodFromId(
      const std::string& input_method_id) const = 0;

  // Changes the current input method to |input_method_id|. If |input_method_id|
  // is not active, switch to the first one in the active input method list.
  virtual void ChangeInputMethod(const std::string& input_method_id) = 0;

  // Enables "login" keyboard layouts (e.g. US Qwerty, US Dvorak, French
  // Azerty) that are necessary for the |language_code| and then switches to
  // |initial_layouts| if the given list is not empty. For example, if
  // |language_code| is "en-US", US Qwerty, US International, US Extended, US
  // Dvorak, and US Colemak layouts would be enabled. Likewise, for Germany
  // locale, US Qwerty which corresponds to the hardware keyboard layout and
  // several keyboard layouts for Germany would be enabled.
  // Only layouts suitable for login screen are enabled.
  virtual void EnableLoginLayouts(
      const std::string& language_code,
      const std::vector<std::string>& initial_layouts) = 0;

  // Activates the input method property specified by the |key|.
  virtual void ActivateInputMethodMenuItem(const std::string& key) = 0;

  // Updates the list of active input method IDs, and then starts or stops the
  // system input method framework as needed.
  virtual bool ReplaceEnabledInputMethods(
      const std::vector<std::string>& new_active_input_method_ids) = 0;

  // Adds one entry to the list of active input method IDs, and then starts or
  // stops the system input method framework as needed.
  virtual bool EnableInputMethod(
      const std::string& new_active_input_method_id) = 0;

  // Adds an input method extension. This function does not takes ownership of
  // |instance|.
  virtual void AddInputMethodExtension(
      const std::string& imm_id,
      InputMethodEngineInterface* instance) = 0;

  // Removes an input method extension.
  virtual void RemoveInputMethodExtension(const std::string& id) = 0;

  // Returns a list of descriptors for all Input Method Extensions.
  virtual void GetInputMethodExtensions(InputMethodDescriptors* result) = 0;

  // Sets the list of extension IME ids which should be enabled.
  virtual void SetEnabledExtensionImes(std::vector<std::string>* ids) = 0;

  // Sets current input method to login default (first owners, then hardware).
  virtual void SetInputMethodLoginDefault() = 0;

  // Sets current input method to login default with the given locale and
  // layout info from VPD.
  virtual void SetInputMethodLoginDefaultFromVPD(
      const std::string& locale, const std::string& layout) = 0;

  // Gets the descriptor of the input method which is currently selected.
  virtual InputMethodDescriptor GetCurrentInputMethod() const = 0;

  virtual bool IsISOLevel5ShiftUsedByCurrentInputMethod() const = 0;

  virtual bool IsAltGrUsedByCurrentInputMethod() const = 0;

  // Returns an X keyboard object which could be used to change the current XKB
  // layout, change the caps lock status, and set the auto repeat rate/interval.
  virtual ImeKeyboard* GetImeKeyboard() = 0;

  // Returns an InputMethodUtil object.
  virtual InputMethodUtil* GetInputMethodUtil() = 0;

  // Returns a ComponentExtentionIMEManager object.
  virtual ComponentExtensionIMEManager* GetComponentExtensionIMEManager() = 0;

  // Switches the current input method (or keyboard layout) to the next one.
  virtual bool SwitchToNextInputMethod() = 0;

  // Switches the current input method (or keyboard layout) to the previous one.
  virtual bool SwitchToPreviousInputMethod(
      const ui::Accelerator& accelerator) = 0;

  // Switches to an input method (or keyboard layout) which is associated with
  // the |accelerator|.
  virtual bool SwitchInputMethod(const ui::Accelerator& accelerator) = 0;

  // If keyboard layout can be uset at login screen
  virtual bool IsLoginKeyboard(const std::string& layout) const = 0;

  // Migrates the input method id to extension-based input method id.
  virtual bool MigrateInputMethods(
      std::vector<std::string>* input_method_ids) = 0;
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROMEOS_IME_INPUT_METHOD_MANAGER_H_
