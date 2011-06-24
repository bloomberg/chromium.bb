// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/input_method_library.h"

#include <algorithm>

#include <glib.h>

#include "unicode/uloc.h"

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/input_method/virtual_keyboard_selector.h"
#include "chrome/browser/chromeos/input_method/xkeyboard.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_service.h"
#include "googleurl/src/gurl.h"

#if !defined(TOUCH_UI)
#include "chrome/browser/chromeos/input_method/candidate_window.h"
#endif

namespace {

const char kIBusDaemonPath[] = "/usr/bin/ibus-daemon";

// Finds a property which has |new_prop.key| from |prop_list|, and replaces the
// property with |new_prop|. Returns true if such a property is found.
bool FindAndUpdateProperty(
    const chromeos::input_method::ImeProperty& new_prop,
    chromeos::input_method::ImePropertyList* prop_list) {
  for (size_t i = 0; i < prop_list->size(); ++i) {
    chromeos::input_method::ImeProperty& prop = prop_list->at(i);
    if (prop.key == new_prop.key) {
      const int saved_id = prop.selection_item_id;
      // Update the list except the radio id. As written in
      // chromeos_input_method.h, |prop.selection_item_id| is dummy.
      prop = new_prop;
      prop.selection_item_id = saved_id;
      return true;
    }
  }
  return false;
}

}  // namespace

namespace chromeos {

// The production implementation of InputMethodLibrary.
class InputMethodLibraryImpl : public InputMethodLibrary,
                               public NotificationObserver,
                               public input_method::IBusController::Observer {
 public:
  InputMethodLibraryImpl()
      : ibus_controller_(NULL),
        should_launch_ime_(false),
        ime_connected_(false),
        defer_ime_startup_(false),
        enable_auto_ime_shutdown_(true),
        ibus_daemon_process_handle_(base::kNullProcessHandle),
#if !defined(TOUCH_UI)
        initialized_successfully_(false),
        candidate_window_controller_(NULL) {
#else
        initialized_successfully_(false) {
#endif
    // Observe APP_TERMINATING to stop input method daemon gracefully.
    // We should not use APP_EXITING here since logout might be canceled by
    // JavaScript after APP_EXITING is sent (crosbug.com/11055).
    // Note that even if we fail to stop input method daemon from
    // Chrome in case of a sudden crash, we have a way to do it from an
    // upstart script. See crosbug.com/6515 and crosbug.com/6995 for
    // details.
    notification_registrar_.Add(this, NotificationType::APP_TERMINATING,
                                NotificationService::AllSources());
  }

  // Initializes the object. On success, returns true on and sets
  // initialized_successfully_ to true.
  //
  // Note that we start monitoring input method status in here in Init()
  // to avoid a potential race. If we start the monitoring right after
  // starting ibus-daemon, there is a higher chance of a race between
  // Chrome and ibus-daemon to occur.
  bool Init() {
    DCHECK(!initialized_successfully_) << "Already initialized";

    ibus_controller_ = input_method::IBusController::Create();
    // The observer should be added before Connect() so we can capture the
    // initial connection change.
    ibus_controller_->AddObserver(this);
    ibus_controller_->Connect();

    initialized_successfully_ = true;
    return true;
  }

  virtual ~InputMethodLibraryImpl() {
    ibus_controller_->RemoveObserver(this);
  }

  virtual void AddObserver(InputMethodLibrary::Observer* observer) {
    if (!observers_.size()) {
      observer->FirstObserverIsAdded(this);
    }
    observers_.AddObserver(observer);
  }

  virtual void RemoveObserver(InputMethodLibrary::Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  virtual void AddVirtualKeyboardObserver(VirtualKeyboardObserver* observer) {
    virtual_keyboard_observers_.AddObserver(observer);
  }

  virtual void RemoveVirtualKeyboardObserver(
      VirtualKeyboardObserver* observer) {
    virtual_keyboard_observers_.RemoveObserver(observer);
  }

  virtual input_method::InputMethodDescriptors* GetActiveInputMethods() {
    input_method::InputMethodDescriptors* result =
        new input_method::InputMethodDescriptors;
    // Build the active input method descriptors from the active input
    // methods cache |active_input_method_ids_|.
    for (size_t i = 0; i < active_input_method_ids_.size(); ++i) {
      const std::string& input_method_id = active_input_method_ids_[i];
      const input_method::InputMethodDescriptor* descriptor =
          input_method::GetInputMethodDescriptorFromId(
              input_method_id);
      if (descriptor) {
        result->push_back(*descriptor);
      } else {
        LOG(ERROR) << "Descriptor is not found for: " << input_method_id;
      }
    }
    // Initially active_input_method_ids_ is empty. In this case, just
    // returns the fallback input method descriptor.
    if (result->empty()) {
      LOG(WARNING) << "No active input methods found.";
      result->push_back(input_method::GetFallbackInputMethodDescriptor());
    }
    return result;
  }

  virtual size_t GetNumActiveInputMethods() {
    scoped_ptr<input_method::InputMethodDescriptors> input_methods(
        GetActiveInputMethods());
    return input_methods->size();
  }

  virtual input_method::InputMethodDescriptors* GetSupportedInputMethods() {
    if (!initialized_successfully_) {
      // If initialization was failed, return the fallback input method,
      // as this function is guaranteed to return at least one descriptor.
      input_method::InputMethodDescriptors* result =
          new input_method::InputMethodDescriptors;
      result->push_back(input_method::GetFallbackInputMethodDescriptor());
      return result;
    }

    // This never returns NULL.
    return input_method::GetSupportedInputMethodDescriptors();
  }

  virtual void ChangeInputMethod(const std::string& input_method_id) {
    // Changing the input method isn't guaranteed to succeed here, but we
    // should remember the last one regardless. See comments in
    // FlushImeConfig() for details.
    tentative_current_input_method_id_ = input_method_id;
    // If the input method daemon is not running and the specified input
    // method is a keyboard layout, switch the keyboard directly.
    if (ibus_daemon_process_handle_ == base::kNullProcessHandle &&
        input_method::IsKeyboardLayout(input_method_id)) {
      // We shouldn't use SetCurrentKeyboardLayoutByName() here. See
      // comments at ChangeCurrentInputMethod() for details.
      ChangeCurrentInputMethodFromId(input_method_id);
    } else {
      // Otherwise, start the input method daemon, and change the input
      // method via the daemon.
      StartInputMethodDaemon();
      // ChangeInputMethodViaIBus() fails if the IBus daemon is not
      // ready yet. In this case, we'll defer the input method change
      // until the daemon is ready.
      if (!ChangeInputMethodViaIBus(input_method_id)) {
        VLOG(1) << "Failed to change the input method to " << input_method_id
                << " (deferring)";
      }
    }
  }

  virtual void SetImePropertyActivated(const std::string& key,
                                       bool activated) {
    if (!initialized_successfully_)
      return;

    DCHECK(!key.empty());
    ibus_controller_->SetImePropertyActivated(key, activated);
  }

  virtual bool InputMethodIsActivated(const std::string& input_method_id) {
    scoped_ptr<input_method::InputMethodDescriptors>
        active_input_method_descriptors(
            GetActiveInputMethods());
    for (size_t i = 0; i < active_input_method_descriptors->size(); ++i) {
      if (active_input_method_descriptors->at(i).id == input_method_id) {
        return true;
      }
    }
    return false;
  }

  virtual bool SetImeConfig(const std::string& section,
                            const std::string& config_name,
                            const input_method::ImeConfigValue& value) {
    // If the config change is for preload engines, update the active
    // input methods cache |active_input_method_ids_| here. We need to
    // update the cache before actually flushing the config. since we need
    // to return active input methods from GetActiveInputMethods() before
    // the input method daemon starts. For instance, we need to show the
    // list of available input methods (keyboard layouts) on the login
    // screen before the input method starts.
    if (section == language_prefs::kGeneralSectionName &&
        config_name == language_prefs::kPreloadEnginesConfigName &&
        value.type == input_method::ImeConfigValue::kValueTypeStringList) {
      active_input_method_ids_ = value.string_list_value;
    }

    // Before calling FlushImeConfig(), start input method process if necessary.
    MaybeStartInputMethodDaemon(section, config_name, value);

    const ConfigKeyType key = std::make_pair(section, config_name);
    current_config_values_[key] = value;
    if (ime_connected_) {
      pending_config_requests_[key] = value;
      FlushImeConfig();
    }

    // Stop input method process if necessary.
    MaybeStopInputMethodDaemon(section, config_name, value);
    // Change the current keyboard layout if necessary.
    MaybeChangeCurrentKeyboardLayout(section, config_name, value);
    return pending_config_requests_.empty();
  }

  virtual input_method::InputMethodDescriptor previous_input_method() const {
    if (previous_input_method_.id.empty()) {
      return input_method::GetFallbackInputMethodDescriptor();
    }
    return previous_input_method_;
  }

  virtual input_method::InputMethodDescriptor current_input_method() const {
    if (current_input_method_.id.empty()) {
      return input_method::GetFallbackInputMethodDescriptor();
    }
    return current_input_method_;
  }

  virtual const input_method::ImePropertyList& current_ime_properties() const {
    return current_ime_properties_;
  }

  virtual std::string GetKeyboardOverlayId(const std::string& input_method_id) {
    if (!initialized_successfully_)
      return "";

    return input_method::GetKeyboardOverlayId(input_method_id);
  }

  virtual void SendHandwritingStroke(
      const input_method::HandwritingStroke& stroke) {
    if (!initialized_successfully_)
      return;
    ibus_controller_->SendHandwritingStroke(stroke);
  }

  virtual void CancelHandwritingStrokes(int stroke_count) {
    if (!initialized_successfully_)
      return;
    // TODO(yusukes): Rename the libcros function to CancelHandwritingStrokes.
    ibus_controller_->CancelHandwriting(stroke_count);
  }

  virtual void RegisterVirtualKeyboard(const GURL& launch_url,
                                       const std::set<std::string>& layouts,
                                       bool is_system) {
    if (!initialized_successfully_)
      return;
    virtual_keyboard_selector_.AddVirtualKeyboard(launch_url,
                                                  layouts,
                                                  is_system);
  }

 private:
  // Returns true if the given input method config value is a single
  // element string list that contains an input method ID of a keyboard
  // layout.
  bool ContainOnlyOneKeyboardLayout(
      const input_method::ImeConfigValue& value) {
    return (value.type == input_method::ImeConfigValue::kValueTypeStringList &&
            value.string_list_value.size() == 1 &&
            input_method::IsKeyboardLayout(
                value.string_list_value[0]));
  }

  // Starts input method daemon based on the |defer_ime_startup_| flag and
  // input method configuration being updated. |section| is a section name of
  // the input method configuration (e.g. "general", "general/hotkey").
  // |config_name| is a name of the configuration (e.g. "preload_engines",
  // "previous_engine"). |value| is the configuration value to be set.
  void MaybeStartInputMethodDaemon(const std::string& section,
                                   const std::string& config_name,
                                   const input_method::ImeConfigValue& value) {
    if (section == language_prefs::kGeneralSectionName &&
        config_name == language_prefs::kPreloadEnginesConfigName &&
        value.type == input_method::ImeConfigValue::kValueTypeStringList &&
        !value.string_list_value.empty()) {
      // If there is only one input method which is a keyboard layout,
      // we don't start the input method processes.  When
      // |defer_ime_startup_| is true, we don't start it either.
      if (ContainOnlyOneKeyboardLayout(value) || defer_ime_startup_) {
        // Do not start the input method daemon.
        return;
      }

      // Otherwise, start the input method daemon.
      const bool just_started = StartInputMethodDaemon();
      if (!just_started) {
        // The daemon is already running.
        // Do not |update tentative_current_input_method_id_|.
        return;
      }

      // The daemon has just been started. To select the initial input method
      // engine correctly, update |tentative_current_input_method_id_|.
      if (tentative_current_input_method_id_.empty()) {
        // Since the |current_input_method_| is in the preloaded engine list,
        // switch to the engine. This is necessary ex. for the following case:
        // 1. "xkb:jp::jpn" is enabled. ibus-daemon is not running.
        // 2. A user enabled "mozc" via DOMUI as well. ibus-daemon is started
        //    and the preloaded engine list is set to "mozc,xkb:jp::jpn".
        // 3. ibus-daemon selects "mozc" as its current engine since "mozc" is
        //    on top of the preloaded engine list.
        // 4. Therefore, we have to change the current engine to "xkb:jp::jpn"
        //    explicitly to avoid unexpected engine switch.
        tentative_current_input_method_id_ = current_input_method_.id;
      }

      if (std::find(value.string_list_value.begin(),
                    value.string_list_value.end(),
                    tentative_current_input_method_id_)
          == value.string_list_value.end()) {
        // The |current_input_method_| is NOT in the preloaded engine list.
        // In this case, ibus-daemon will automatically select the first engine
        // in the list, |value.string_list_value[0]|, and send global engine
        // changed signal to Chrome. See crosbug.com/13406.
        tentative_current_input_method_id_.clear();
      }
    }
  }

  // Stops input method daemon based on the |enable_auto_ime_shutdown_| flag
  // and input method configuration being updated.
  // See also: MaybeStartInputMethodDaemon().
  void MaybeStopInputMethodDaemon(const std::string& section,
                                  const std::string& config_name,
                                  const input_method::ImeConfigValue& value) {
    // If there is only one input method which is a keyboard layout,
    // and |enable_auto_ime_shutdown_| is true, we'll stop the input
    // method daemon.
    if (section == language_prefs::kGeneralSectionName &&
        config_name == language_prefs::kPreloadEnginesConfigName &&
        ContainOnlyOneKeyboardLayout(value) &&
        enable_auto_ime_shutdown_) {
      StopInputMethodDaemon();
    }
  }

  // Change the keyboard layout per input method configuration being
  // updated, if necessary. See also: MaybeStartInputMethodDaemon().
  void MaybeChangeCurrentKeyboardLayout(
      const std::string& section,
      const std::string& config_name,
      const input_method::ImeConfigValue& value) {

    // If there is only one input method which is a keyboard layout, we'll
    // change the keyboard layout per the only one input method now
    // available.
    if (section == language_prefs::kGeneralSectionName &&
        config_name == language_prefs::kPreloadEnginesConfigName &&
        ContainOnlyOneKeyboardLayout(value)) {
      // We shouldn't use SetCurrentKeyboardLayoutByName() here. See
      // comments at ChangeCurrentInputMethod() for details.
      ChangeCurrentInputMethodFromId(value.string_list_value[0]);
    }
  }

  // Changes the current input method to |input_method_id| via IBus
  // daemon. If the id is not in the preload_engine list, this function
  // changes the current method to the first preloaded engine. Returns
  // true if the current engine is switched to |input_method_id| or the
  // first one.
  bool ChangeInputMethodViaIBus(const std::string& input_method_id) {
    if (!initialized_successfully_)
      return false;

    std::string input_method_id_to_switch = input_method_id;

    if (!InputMethodIsActivated(input_method_id)) {
      // This path might be taken if prefs::kLanguageCurrentInputMethod (NOT
      // synced with cloud) and kLanguagePreloadEngines (synced with cloud) are
      // mismatched. e.g. the former is 'xkb:us::eng' and the latter (on the
      // sync server) is 'xkb:jp::jpn,mozc'.
      scoped_ptr<input_method::InputMethodDescriptors> input_methods(
          GetActiveInputMethods());
      DCHECK(!input_methods->empty());
      if (!input_methods->empty()) {
        input_method_id_to_switch = input_methods->at(0).id;
        LOG(INFO) << "Can't change the current input method to "
                  << input_method_id << " since the engine is not preloaded. "
                  << "Switch to " << input_method_id_to_switch << " instead.";
      }
    }

    if (ibus_controller_->ChangeInputMethod(input_method_id_to_switch)) {
      return true;
    }

    // ChangeInputMethod() fails if the IBus daemon is not yet ready.
    LOG(ERROR) << "Can't switch input method to " << input_method_id_to_switch;
    return false;
  }

  // Flushes the input method config data. The config data is queued up in
  // |pending_config_requests_| until the config backend (ibus-memconf)
  // starts.
  void FlushImeConfig() {
    if (!initialized_successfully_)
      return;

    bool active_input_methods_are_changed = false;
    InputMethodConfigRequests::iterator iter =
        pending_config_requests_.begin();
    while (iter != pending_config_requests_.end()) {
      const std::string& section = iter->first.first;
      const std::string& config_name = iter->first.second;
      input_method::ImeConfigValue& value = iter->second;

      if (config_name == language_prefs::kPreloadEnginesConfigName &&
          !tentative_current_input_method_id_.empty()) {
        // We should use |tentative_current_input_method_id_| as the initial
        // active input method for the following reasons:
        //
        // 1) Calls to ChangeInputMethod() will fail if the input method has not
        // yet been added to preload_engines.  As such, the call is deferred
        // until after all config values have been sent to the IME process.
        //
        // 2) We might have already changed the current input method to one
        // of XKB layouts without going through the IBus daemon (we can do
        // it without the IBus daemon started).
        std::vector<std::string>::iterator engine_iter = std::find(
            value.string_list_value.begin(),
            value.string_list_value.end(),
            tentative_current_input_method_id_);
        if (engine_iter != value.string_list_value.end()) {
          // Use std::rotate to keep the relative order of engines the same e.g.
          // from "A,B,C" to "C,A,B".
          // We don't have to |active_input_method_ids_|, which decides the
          // order of engines in the switcher menu, since the relative order
          // of |value.string_list_value| is not changed.
          std::rotate(value.string_list_value.begin(),
                      engine_iter,  // this becomes the new first element
                      value.string_list_value.end());
        } else {
          LOG(WARNING) << tentative_current_input_method_id_
                       << " is not in preload_engines: " << value.ToString();
        }
        tentative_current_input_method_id_.erase();
      }

      if (ibus_controller_->SetImeConfig(section, config_name, value)) {
        // Check if it's a change in active input methods.
        if (config_name == language_prefs::kPreloadEnginesConfigName) {
          active_input_methods_are_changed = true;
          VLOG(1) << "Updated preload_engines: " << value.ToString();
        }
        // Successfully sent. Remove the command and proceed to the next one.
        pending_config_requests_.erase(iter++);
      } else {
        // If SetImeConfig() fails, subsequent calls will likely fail.
        break;
      }
    }

    // Notify the current input method and the number of active input methods to
    // the UI so that the UI could determine e.g. if it should show/hide the
    // input method indicator, etc. We have to call FOR_EACH_OBSERVER here since
    // updating "preload_engine" does not necessarily trigger a DBus signal such
    // as "global-engine-changed". For example,
    // 1) If we change the preload_engine from "xkb:us:intl:eng" (i.e. the
    //    indicator is hidden) to "xkb:us:intl:eng,mozc", we have to update UI
    //    so it shows the indicator, but no signal is sent from ibus-daemon
    //    because the current input method is not changed.
    // 2) If we change the preload_engine from "xkb:us::eng,mozc" (i.e. the
    //    indicator is shown and ibus-daemon is started) to "xkb:us::eng", we
    //    have to update UI so it hides the indicator, but we should not expect
    //    that ibus-daemon could send a DBus signal since the daemon is killed
    //    right after this FlushImeConfig() call.
    if (active_input_methods_are_changed) {
      // The |current_input_method_| member might be stale here as
      // SetImeConfig("preload_engine") call above might change the
      // current input method in ibus-daemon (ex. this occurs when the
      // input method currently in use is removed from the options
      // page). However, it should be safe to use the member here,
      // for the following reasons:
      // 1. If ibus-daemon is to be killed, we'll switch to the only one
      //    keyboard layout, and observers are notified. See
      //    MaybeStopInputMethodDaemon() for details.
      // 2. Otherwise, "global-engine-changed" signal is delivered from
      //    ibus-daemon, and observers are notified. See
      //    InputMethodChangedHandler() for details.
      const size_t num_active_input_methods = GetNumActiveInputMethods();
      FOR_EACH_OBSERVER(InputMethodLibrary::Observer, observers_,
                        ActiveInputMethodsChanged(this,
                                                  current_input_method_,
                                                  num_active_input_methods));
    }
  }

  // IBusController override.
  virtual void OnCurrentInputMethodChanged(
      const input_method::InputMethodDescriptor& current_input_method) {
    // The handler is called when the input method method change is
    // notified via a DBus connection. Since the DBus notificatiosn are
    // handled in the UI thread, we can assume that this function always
    // runs on the UI thread, but just in case.
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      LOG(ERROR) << "Not on UI thread";
      return;
    }

    ChangeCurrentInputMethod(current_input_method);
  }

  // IBusController override.
  virtual void OnRegisterImeProperties(
      const input_method::ImePropertyList& prop_list) {
    // See comments in InputMethodChangedHandler.
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      LOG(ERROR) << "Not on UI thread";
      return;
    }

    RegisterProperties(prop_list);
  }

  // IBusController override.
  virtual void OnUpdateImeProperty(
      const input_method::ImePropertyList& prop_list) {
    // See comments in InputMethodChangedHandler.
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      LOG(ERROR) << "Not on UI thread";
      return;
    }

    UpdateProperty(prop_list);
  }

  // IBusController override.
  virtual void OnConnectionChange(bool connected) {
    // See comments in InputMethodChangedHandler.
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      LOG(ERROR) << "Not on UI thread";
      return;
    }

    ime_connected_ = connected;
    if (connected) {
      pending_config_requests_.clear();
      pending_config_requests_.insert(
          current_config_values_.begin(),
          current_config_values_.end());
      FlushImeConfig();

      ChangeInputMethod(previous_input_method().id);
      ChangeInputMethod(current_input_method().id);
    }
  }

  // Changes the current input method from the given input method
  // descriptor.  This function updates states like current_input_method_
  // and notifies observers about the change (that will update the
  // preferences), hence this function should always be used even if you
  // just need to change the current keyboard layout.
  void ChangeCurrentInputMethod(const input_method::InputMethodDescriptor&
                                new_input_method) {
    if (current_input_method_.id != new_input_method.id) {
      previous_input_method_ = current_input_method_;
      current_input_method_ = new_input_method;

      // Change the keyboard layout to a preferred layout for the input method.
      if (!input_method::SetCurrentKeyboardLayoutByName(
              current_input_method_.keyboard_layout)) {
        LOG(ERROR) << "Failed to change keyboard layout to "
                   << current_input_method_.keyboard_layout;
      }

      // Ask the first observer to update preferences. We should not ask every
      // observer to do so. Otherwise, we'll end up updating preferences many
      // times when many observers are attached (ex. many windows are opened),
      // which is unnecessary and expensive.
      ObserverListBase<InputMethodLibrary::Observer>::Iterator it(observers_);
      InputMethodLibrary::Observer* first_observer = it.GetNext();
      if (first_observer) {
        first_observer->PreferenceUpdateNeeded(this,
                                               previous_input_method_,
                                               current_input_method_);
      }
    }

    // Update input method indicators (e.g. "US", "DV") in Chrome windows.
    // For now, we have to do this every time to keep indicators updated. See
    // comments near the FOR_EACH_OBSERVER call in FlushImeConfig() for details.
    const size_t num_active_input_methods = GetNumActiveInputMethods();
    FOR_EACH_OBSERVER(InputMethodLibrary::Observer, observers_,
                      InputMethodChanged(this,
                                         current_input_method_,
                                         num_active_input_methods));

    // Update virtual keyboard.
#if defined(TOUCH_UI)
    const input_method::VirtualKeyboard* virtual_keyboard = NULL;
    std::string virtual_keyboard_layout = "";

    const std::vector<std::string>& layouts_vector
        = current_input_method_.virtual_keyboard_layouts();
    std::vector<std::string>::const_iterator iter;
    for (iter = layouts_vector.begin(); iter != layouts_vector.end(); ++iter) {
      virtual_keyboard =
          virtual_keyboard_selector_.SelectVirtualKeyboard(*iter);
      if (virtual_keyboard) {
        virtual_keyboard_layout = *iter;
        break;
      }
    }

    if (!virtual_keyboard) {
      // The system virtual keyboard does not support some XKB layouts? or
      // a third-party input method engine uses a wrong virtual keyboard
      // layout name? Fallback to the default layout.
      LOG(ERROR) << "Could not find a virtual keyboard for "
                 << current_input_method_.id;

      // If the hardware is for US, show US Qwerty virtual keyboard.
      // If it's for France, show Azerty one.
      const std::string fallback_id =
          input_method::GetHardwareInputMethodId();
      const input_method::InputMethodDescriptor* fallback_desc =
          input_method::GetInputMethodDescriptorFromId(fallback_id);

      DCHECK(fallback_desc);
      const std::vector<std::string>& fallback_layouts_vector =
          fallback_desc->virtual_keyboard_layouts();

      for (iter = fallback_layouts_vector.begin();
           iter != fallback_layouts_vector.end();
           ++iter) {
        virtual_keyboard =
            virtual_keyboard_selector_.SelectVirtualKeyboard(*iter);
        if (virtual_keyboard) {
          virtual_keyboard_layout = *iter;
          LOG(ERROR) << "Fall back to '" << (*iter) << "' virtual keyboard";
          break;
        }
      }
    }

    if (!virtual_keyboard) {
      // kFallbackVirtualKeyboardLayout should always be supported by one of the
      // system virtual keyboards.
      static const char kFallbackVirtualKeyboardLayout[] = "us";

      LOG(ERROR) << "Could not find a FALLBACK virtual keyboard for "
                 << current_input_method_.id
                 << ". Use '" << kFallbackVirtualKeyboardLayout
                 << "' virtual keyboard";
      virtual_keyboard = virtual_keyboard_selector_.SelectVirtualKeyboard(
          kFallbackVirtualKeyboardLayout);
      virtual_keyboard_layout = kFallbackVirtualKeyboardLayout;
    }

    if (virtual_keyboard) {
      FOR_EACH_OBSERVER(VirtualKeyboardObserver, virtual_keyboard_observers_,
                        VirtualKeyboardChanged(this,
                                               *virtual_keyboard,
                                               virtual_keyboard_layout));
    }
#endif  // TOUCH_UI
  }

  // Changes the current input method from the given input method ID.
  // This function is just a wrapper of ChangeCurrentInputMethod().
  void ChangeCurrentInputMethodFromId(const std::string& input_method_id) {
    const input_method::InputMethodDescriptor* descriptor =
        input_method::GetInputMethodDescriptorFromId(
            input_method_id);
    if (descriptor) {
      ChangeCurrentInputMethod(*descriptor);
    } else {
      LOG(ERROR) << "Descriptor is not found for: " << input_method_id;
    }
  }

  // Registers the properties used by the current input method.
  void RegisterProperties(const input_method::ImePropertyList& prop_list) {
    // |prop_list| might be empty. This means "clear all properties."
    current_ime_properties_ = prop_list;

    // Update input method menu
    FOR_EACH_OBSERVER(InputMethodLibrary::Observer, observers_,
                      PropertyListChanged(this,
                                          current_ime_properties_));
  }

  // Starts the input method daemon. Unlike MaybeStopInputMethodDaemon(),
  // this function always starts the daemon. Returns true if the daemon is
  // started. Otherwise, e.g. the daemon is already started, returns false.
  bool StartInputMethodDaemon() {
    should_launch_ime_ = true;
    return MaybeLaunchInputMethodDaemon();
  }

  // Updates the properties used by the current input method.
  void UpdateProperty(const input_method::ImePropertyList& prop_list) {
    for (size_t i = 0; i < prop_list.size(); ++i) {
      FindAndUpdateProperty(prop_list[i], &current_ime_properties_);
    }

    // Update input method menu
    FOR_EACH_OBSERVER(InputMethodLibrary::Observer, observers_,
                      PropertyListChanged(this,
                                          current_ime_properties_));
  }

  // Launches an input method procsess specified by the given command
  // line. On success, returns true and stores the process handle in
  // |process_handle|. Otherwise, returns false, and the contents of
  // |process_handle| is untouched. OnImeShutdown will be called when the
  // process terminates.
  bool LaunchInputMethodProcess(const std::string& command_line,
                                base::ProcessHandle* process_handle) {
    std::vector<std::string> argv;
    base::file_handle_mapping_vector fds_to_remap;
    base::ProcessHandle handle = base::kNullProcessHandle;

    // TODO(zork): export "LD_PRELOAD=/usr/lib/libcrash.so"
    base::SplitString(command_line, ' ', &argv);
    const bool result = base::LaunchApp(argv,
                                        fds_to_remap,  // no remapping
                                        false,  // wait
                                        &handle);
    if (!result) {
      LOG(ERROR) << "Could not launch: " << command_line;
      return false;
    }

    // g_child_watch_add is necessary to prevent the process from becoming a
    // zombie.
    // TODO(yusukes): port g_child_watch_add to base/process_utils_posix.cc.
    const base::ProcessId pid = base::GetProcId(handle);
    g_child_watch_add(pid,
                      reinterpret_cast<GChildWatchFunc>(OnImeShutdown),
                      this);

    *process_handle = handle;
    VLOG(1) << command_line << " (PID=" << pid << ") is started";
    return  true;
  }

  // Launches input method daemon if these are not yet running. Returns true if
  // the daemon is started. Otherwise, e.g. the daemon is already started,
  // returns false.
  bool MaybeLaunchInputMethodDaemon() {
    // CandidateWindowController requires libcros to be loaded. Besides,
    // launching ibus-daemon without libcros loaded doesn't make sense.
    if (!initialized_successfully_)
      return false;

    if (!should_launch_ime_) {
      return false;
    }

#if !defined(TOUCH_UI)
    if (!candidate_window_controller_.get()) {
      candidate_window_controller_.reset(
          new input_method::CandidateWindowController);
      if (!candidate_window_controller_->Init()) {
        LOG(WARNING) << "Failed to initialize the candidate window controller";
      }
    }
#endif

    if (ibus_daemon_process_handle_ != base::kNullProcessHandle) {
      return false;  // ibus-daemon is already running.
    }

    // TODO(zork): Send output to /var/log/ibus.log
    const std::string ibus_daemon_command_line =
        StringPrintf("%s --panel=disable --cache=none --restart --replace",
                     kIBusDaemonPath);
    if (!LaunchInputMethodProcess(
            ibus_daemon_command_line, &ibus_daemon_process_handle_)) {
      LOG(ERROR) << "Failed to launch " << ibus_daemon_command_line;
      return false;
    }
    return true;
  }

  // Called when the input method process is shut down.
  static void OnImeShutdown(GPid pid,
                            gint status,
                            InputMethodLibraryImpl* library) {
    if (library->ibus_daemon_process_handle_ != base::kNullProcessHandle &&
        base::GetProcId(library->ibus_daemon_process_handle_) == pid) {
      library->ibus_daemon_process_handle_ = base::kNullProcessHandle;
    }

    // Restart input method daemon if needed.
    library->MaybeLaunchInputMethodDaemon();
  }

  // Stops the backend input method daemon. This function should also be
  // called from MaybeStopInputMethodDaemon(), except one case where we
  // stop the input method daemon at Chrome shutdown in Observe().
  void StopInputMethodDaemon() {
    if (!initialized_successfully_)
      return;

    should_launch_ime_ = false;
    if (ibus_daemon_process_handle_ != base::kNullProcessHandle) {
      const base::ProcessId pid = base::GetProcId(ibus_daemon_process_handle_);
      if (!ibus_controller_->StopInputMethodProcess()) {
        LOG(ERROR) << "StopInputMethodProcess IPC failed. Sending SIGTERM to "
                   << "PID " << pid;
        base::KillProcess(ibus_daemon_process_handle_, -1, false /* wait */);
      }
      VLOG(1) << "ibus-daemon (PID=" << pid << ") is terminated";
      ibus_daemon_process_handle_ = base::kNullProcessHandle;
    }
  }

  void SetDeferImeStartup(bool defer) {
    VLOG(1) << "Setting DeferImeStartup to " << defer;
    defer_ime_startup_ = defer;
  }

  void SetEnableAutoImeShutdown(bool enable) {
    enable_auto_ime_shutdown_ = enable;
  }

  // NotificationObserver implementation:
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details) {
    // Stop the input method daemon on browser shutdown.
    if (type.value == NotificationType::APP_TERMINATING) {
      notification_registrar_.RemoveAll();
      StopInputMethodDaemon();
#if !defined(TOUCH_UI)
      candidate_window_controller_.reset(NULL);
#endif
    }
  }

  // A reference to the language api, to allow callbacks when the input method
  // status changes.
  input_method::IBusController* ibus_controller_;
  ObserverList<InputMethodLibrary::Observer> observers_;
  ObserverList<VirtualKeyboardObserver> virtual_keyboard_observers_;

  // The input method which was/is selected.
  input_method::InputMethodDescriptor previous_input_method_;
  input_method::InputMethodDescriptor current_input_method_;

  // The input method properties which the current input method uses. The list
  // might be empty when no input method is used.
  input_method::ImePropertyList current_ime_properties_;

  typedef std::pair<std::string, std::string> ConfigKeyType;
  typedef std::map<ConfigKeyType,
        input_method::ImeConfigValue> InputMethodConfigRequests;
  // SetImeConfig requests that are not yet completed.
  // Use a map to queue config requests, so we only send the last request for
  // the same config key (i.e. we'll discard ealier requests for the same
  // config key). As we discard old requests for the same config key, the order
  // of requests doesn't matter, so it's safe to use a map.
  InputMethodConfigRequests pending_config_requests_;

  // Values that have been set via SetImeConfig().  We keep a copy available to
  // resend if the ime restarts and loses its state.
  InputMethodConfigRequests current_config_values_;

  // This is used to register this object to APP_EXITING notification.
  NotificationRegistrar notification_registrar_;

  // True if we should launch the input method daemon.
  bool should_launch_ime_;
  // True if the connection to the IBus daemon is alive.
  bool ime_connected_;
  // If true, we'll defer the startup until a non-default method is
  // activated.
  bool defer_ime_startup_;
  // True if we should stop input method daemon when there are no input
  // methods other than one for the hardware keyboard.
  bool enable_auto_ime_shutdown_;
  // The ID of the tentative current input method (ex. "mozc"). This value
  // can be different from the actual current input method, if
  // ChangeInputMethod() fails.
  // TODO(yusukes): clear this variable when a user logs in.
  std::string tentative_current_input_method_id_;

  // The process handle of the IBus daemon. kNullProcessHandle if it's not
  // running.
  base::ProcessHandle ibus_daemon_process_handle_;

  // True if initialization is successfully done, meaning that libcros is
  // loaded and input method status monitoring is started. This value
  // should be checked where we call libcros functions.
  bool initialized_successfully_;

  // The candidate window.  This will be deleted when the APP_TERMINATING
  // message is sent.
#if !defined(TOUCH_UI)
  scoped_ptr<input_method::CandidateWindowController>
      candidate_window_controller_;
#endif

  // An object which keeps a list of available virtual keyboards.
  input_method::VirtualKeyboardSelector virtual_keyboard_selector_;

  // The active input method ids cache.
  std::vector<std::string> active_input_method_ids_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodLibraryImpl);
};

// The stub implementation of InputMethodLibrary. Used for testing.
class InputMethodLibraryStubImpl : public InputMethodLibrary {
 public:
  InputMethodLibraryStubImpl()
      : keyboard_overlay_map_(GetKeyboardOverlayMapForTesting()) {
    current_input_method_ = input_method::GetFallbackInputMethodDescriptor();
  }

  virtual ~InputMethodLibraryStubImpl() {}
  virtual void AddObserver(Observer* observer) {}
  virtual void AddVirtualKeyboardObserver(VirtualKeyboardObserver* observer) {}
  virtual void RemoveObserver(Observer* observer) {}
  virtual void RemoveVirtualKeyboardObserver(
      VirtualKeyboardObserver* observer) {}

  virtual input_method::InputMethodDescriptors* GetActiveInputMethods() {
    return GetInputMethodDescriptorsForTesting();
  }


  virtual size_t GetNumActiveInputMethods() {
    scoped_ptr<input_method::InputMethodDescriptors> descriptors(
        GetActiveInputMethods());
    return descriptors->size();
  }

  virtual input_method::InputMethodDescriptors* GetSupportedInputMethods() {
    return GetInputMethodDescriptorsForTesting();
  }

  virtual void ChangeInputMethod(const std::string& input_method_id) {}
  virtual void SetImePropertyActivated(const std::string& key,
                                       bool activated) {}

  virtual bool InputMethodIsActivated(const std::string& input_method_id) {
    return true;
  }

  virtual bool SetImeConfig(const std::string& section,
                            const std::string& config_name,
                            const input_method::ImeConfigValue& value) {
    return false;
  }

  virtual input_method::InputMethodDescriptor previous_input_method() const {
    return previous_input_method_;
  }

  virtual input_method::InputMethodDescriptor current_input_method() const {
    return current_input_method_;
  }

  virtual const input_method::ImePropertyList& current_ime_properties() const {
    return current_ime_properties_;
  }

  virtual bool StartInputMethodDaemon() {
    return true;
  }
  virtual void StopInputMethodDaemon() {}
  virtual void SetDeferImeStartup(bool defer) {}
  virtual void SetEnableAutoImeShutdown(bool enable) {}

  virtual std::string GetKeyboardOverlayId(const std::string& input_method_id) {
    KeyboardOverlayMap::const_iterator iter =
        keyboard_overlay_map_->find(input_method_id);
    return (iter != keyboard_overlay_map_->end()) ?
        iter->second : "";
  }

  virtual void SendHandwritingStroke(
      const input_method::HandwritingStroke& stroke) {}
  virtual void CancelHandwritingStrokes(int stroke_count) {}
  virtual void RegisterVirtualKeyboard(const GURL& launch_url,
                                       const std::set<std::string>& layouts,
                                       bool is_system) {}

 private:
  typedef std::map<std::string, std::string> KeyboardOverlayMap;

  // Gets input method descriptors for testing. Shouldn't be used for
  // production.
  input_method::InputMethodDescriptors* GetInputMethodDescriptorsForTesting() {
    input_method::InputMethodDescriptors* descriptions =
        new input_method::InputMethodDescriptors;
    // The list is created from output of gen_engines.py in libcros.
    // % SHARE=/build/x86-generic/usr/share python gen_engines.py
    // $SHARE/chromeos-assets/input_methods/whitelist.txt
    // $SHARE/ibus/component/{chewing,hangul,m17n,mozc,pinyin,xkb-layouts}.xml
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:nl::nld", "Netherlands", "nl", "nl", "nld"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:be::nld", "Belgium", "be", "be", "nld"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:fr::fra", "France", "fr", "fr", "fra"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:be::fra", "Belgium", "be", "be", "fra"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:ca::fra", "Canada", "ca", "ca", "fra"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:ch:fr:fra", "Switzerland - French", "ch(fr)", "ch(fr)", "fra"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:de::ger", "Germany", "de", "de", "ger"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:de:neo:ger", "Germany - Neo 2", "de(neo)", "de(neo)", "ger"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:be::ger", "Belgium", "be", "be", "ger"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:ch::ger", "Switzerland", "ch", "ch", "ger"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "mozc", "Mozc (US keyboard layout)", "us", "us", "ja"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "mozc-jp", "Mozc (Japanese keyboard layout)", "jp", "jp", "ja"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "mozc-dv",
        "Mozc (US Dvorak keyboard layout)", "us(dvorak)", "us(dvorak)", "ja"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:jp::jpn", "Japan", "jp", "jp", "jpn"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:ru::rus", "Russia", "ru", "ru", "rus"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:ru:phonetic:rus",
        "Russia - Phonetic", "ru(phonetic)", "ru(phonetic)", "rus"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "m17n:th:kesmanee", "kesmanee (m17n)", "us", "us", "th"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "m17n:th:pattachote", "pattachote (m17n)", "us", "us", "th"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "m17n:th:tis820", "tis820 (m17n)", "us", "us", "th"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "mozc-chewing", "Mozc Chewing (Chewing)", "us", "us", "zh_TW"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "m17n:zh:cangjie", "cangjie (m17n)", "us", "us", "zh"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "m17n:zh:quick", "quick (m17n)", "us", "us", "zh"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "m17n:vi:tcvn", "tcvn (m17n)", "us", "us", "vi"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "m17n:vi:telex", "telex (m17n)", "us", "us", "vi"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "m17n:vi:viqr", "viqr (m17n)", "us", "us", "vi"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "m17n:vi:vni", "vni (m17n)", "us", "us", "vi"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:us::eng", "USA", "us", "us", "eng"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:us:intl:eng",
        "USA - International (with dead keys)", "us(intl)", "us(intl)", "eng"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:us:altgr-intl:eng", "USA - International (AltGr dead keys)",
        "us(altgr-intl)", "us(altgr-intl)", "eng"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:us:dvorak:eng",
        "USA - Dvorak", "us(dvorak)", "us(dvorak)", "eng"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:us:colemak:eng",
        "USA - Colemak", "us(colemak)", "us(colemak)", "eng"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "hangul", "Korean", "kr(kr104)", "kr(kr104)", "ko"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "pinyin", "Pinyin", "us", "us", "zh"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "pinyin-dv", "Pinyin (for US Dvorak keyboard)",
        "us(dvorak)", "us(dvorak)", "zh"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "m17n:ar:kbd", "kbd (m17n)", "us", "us", "ar"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "m17n:hi:itrans", "itrans (m17n)", "us", "us", "hi"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "m17n:fa:isiri", "isiri (m17n)", "us", "us", "fa"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:br::por", "Brazil", "br", "br", "por"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:bg::bul", "Bulgaria", "bg", "bg", "bul"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:bg:phonetic:bul", "Bulgaria - Traditional phonetic",
        "bg(phonetic)", "bg(phonetic)", "bul"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:ca:eng:eng", "Canada - English", "ca(eng)", "ca(eng)", "eng"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:cz::cze", "Czechia", "cz", "cz", "cze"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:ee::est", "Estonia", "ee", "ee", "est"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:es::spa", "Spain", "es", "es", "spa"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:es:cat:cat", "Spain - Catalan variant with middle-dot L",
        "es(cat)", "es(cat)", "cat"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:dk::dan", "Denmark", "dk", "dk", "dan"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:gr::gre", "Greece", "gr", "gr", "gre"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:il::heb", "Israel", "il", "il", "heb"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:kr:kr104:kor", "Korea, Republic of - 101/104 key Compatible",
        "kr(kr104)", "kr(kr104)", "kor"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:latam::spa", "Latin American", "latam", "latam", "spa"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:lt::lit", "Lithuania", "lt", "lt", "lit"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:lv:apostrophe:lav", "Latvia - Apostrophe (') variant",
        "lv(apostrophe)", "lv(apostrophe)", "lav"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:hr::scr", "Croatia", "hr", "hr", "scr"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:gb:extd:eng", "United Kingdom - Extended - Winkeys",
        "gb(extd)", "gb(extd)", "eng"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:gb:dvorak:eng", "United Kingdom - Dvorak",
        "gb(dvorak)", "gb(dvorak)", "eng"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:fi::fin", "Finland", "fi", "fi", "fin"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:hu::hun", "Hungary", "hu", "hu", "hun"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:it::ita", "Italy", "it", "it", "ita"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:no::nob", "Norway", "no", "no", "nob"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:pl::pol", "Poland", "pl", "pl", "pol"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:pt::por", "Portugal", "pt", "pt", "por"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:ro::rum", "Romania", "ro", "ro", "rum"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:se::swe", "Sweden", "se", "se", "swe"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:sk::slo", "Slovakia", "sk", "sk", "slo"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:si::slv", "Slovenia", "si", "si", "slv"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:rs::srp", "Serbia", "rs", "rs", "srp"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:tr::tur", "Turkey", "tr", "tr", "tur"));
    descriptions->push_back(input_method::InputMethodDescriptor(
        "xkb:ua::ukr", "Ukraine", "ua", "ua", "ukr"));
    return descriptions;
  }

  // Gets keyboard overlay map for testing. Shouldn't be used for
  // production.
  std::map<std::string, std::string>* GetKeyboardOverlayMapForTesting() {
    KeyboardOverlayMap* keyboard_overlay_map =
        new KeyboardOverlayMap;
    (*keyboard_overlay_map)["xkb:nl::nld"] = "nl";
    (*keyboard_overlay_map)["xkb:be::nld"] = "nl";
    (*keyboard_overlay_map)["xkb:fr::fra"] = "fr";
    (*keyboard_overlay_map)["xkb:be::fra"] = "fr";
    (*keyboard_overlay_map)["xkb:ca::fra"] = "fr_CA";
    (*keyboard_overlay_map)["xkb:ch:fr:fra"] = "fr";
    (*keyboard_overlay_map)["xkb:de::ger"] = "de";
    (*keyboard_overlay_map)["xkb:be::ger"] = "de";
    (*keyboard_overlay_map)["xkb:ch::ger"] = "de";
    (*keyboard_overlay_map)["mozc"] = "en_US";
    (*keyboard_overlay_map)["mozc-jp"] = "ja";
    (*keyboard_overlay_map)["mozc-dv"] = "en_US_dvorak";
    (*keyboard_overlay_map)["xkb:jp::jpn"] = "ja";
    (*keyboard_overlay_map)["xkb:ru::rus"] = "ru";
    (*keyboard_overlay_map)["xkb:ru:phonetic:rus"] = "ru";
    (*keyboard_overlay_map)["m17n:th:kesmanee"] = "th";
    (*keyboard_overlay_map)["m17n:th:pattachote"] = "th";
    (*keyboard_overlay_map)["m17n:th:tis820"] = "th";
    (*keyboard_overlay_map)["mozc-chewing"] = "zh_TW";
    (*keyboard_overlay_map)["m17n:zh:cangjie"] = "zh_TW";
    (*keyboard_overlay_map)["m17n:zh:quick"] = "zh_TW";
    (*keyboard_overlay_map)["m17n:vi:tcvn"] = "vi";
    (*keyboard_overlay_map)["m17n:vi:telex"] = "vi";
    (*keyboard_overlay_map)["m17n:vi:viqr"] = "vi";
    (*keyboard_overlay_map)["m17n:vi:vni"] = "vi";
    (*keyboard_overlay_map)["xkb:us::eng"] = "en_US";
    (*keyboard_overlay_map)["xkb:us:intl:eng"] = "en_US";
    (*keyboard_overlay_map)["xkb:us:altgr-intl:eng"] = "en_US";
    (*keyboard_overlay_map)["xkb:us:dvorak:eng"] =
        "en_US_dvorak";
    (*keyboard_overlay_map)["xkb:us:colemak:eng"] =
        "en_US";
    (*keyboard_overlay_map)["hangul"] = "ko";
    (*keyboard_overlay_map)["pinyin"] = "zh_CN";
    (*keyboard_overlay_map)["m17n:ar:kbd"] = "ar";
    (*keyboard_overlay_map)["m17n:hi:itrans"] = "hi";
    (*keyboard_overlay_map)["m17n:fa:isiri"] = "ar";
    (*keyboard_overlay_map)["xkb:br::por"] = "pt_BR";
    (*keyboard_overlay_map)["xkb:bg::bul"] = "bg";
    (*keyboard_overlay_map)["xkb:bg:phonetic:bul"] = "bg";
    (*keyboard_overlay_map)["xkb:ca:eng:eng"] = "ca";
    (*keyboard_overlay_map)["xkb:cz::cze"] = "cs";
    (*keyboard_overlay_map)["xkb:ee::est"] = "et";
    (*keyboard_overlay_map)["xkb:es::spa"] = "es";
    (*keyboard_overlay_map)["xkb:es:cat:cat"] = "ca";
    (*keyboard_overlay_map)["xkb:dk::dan"] = "da";
    (*keyboard_overlay_map)["xkb:gr::gre"] = "el";
    (*keyboard_overlay_map)["xkb:il::heb"] = "iw";
    (*keyboard_overlay_map)["xkb:kr:kr104:kor"] = "ko";
    (*keyboard_overlay_map)["xkb:latam::spa"] = "es_419";
    (*keyboard_overlay_map)["xkb:lt::lit"] = "lt";
    (*keyboard_overlay_map)["xkb:lv:apostrophe:lav"] = "lv";
    (*keyboard_overlay_map)["xkb:hr::scr"] = "hr";
    (*keyboard_overlay_map)["xkb:gb:extd:eng"] = "en_GB";
    (*keyboard_overlay_map)["xkb:gb:dvorak:eng"] = "en_GB_dvorak";
    (*keyboard_overlay_map)["xkb:fi::fin"] = "fi";
    (*keyboard_overlay_map)["xkb:hu::hun"] = "hu";
    (*keyboard_overlay_map)["xkb:it::ita"] = "it";
    (*keyboard_overlay_map)["xkb:no::nob"] = "no";
    (*keyboard_overlay_map)["xkb:pl::pol"] = "pl";
    (*keyboard_overlay_map)["xkb:pt::por"] = "pt_PT";
    (*keyboard_overlay_map)["xkb:ro::rum"] = "ro";
    (*keyboard_overlay_map)["xkb:se::swe"] = "sv";
    (*keyboard_overlay_map)["xkb:sk::slo"] = "sk";
    (*keyboard_overlay_map)["xkb:si::slv"] = "sl";
    (*keyboard_overlay_map)["xkb:rs::srp"] = "sr";
    (*keyboard_overlay_map)["xkb:tr::tur"] = "tr";
    (*keyboard_overlay_map)["xkb:ua::ukr"] = "uk";
    return keyboard_overlay_map;
  }

  input_method::InputMethodDescriptor previous_input_method_;
  input_method::InputMethodDescriptor current_input_method_;
  input_method::ImePropertyList current_ime_properties_;
  scoped_ptr<KeyboardOverlayMap> keyboard_overlay_map_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodLibraryStubImpl);
};

// static
InputMethodLibrary* InputMethodLibrary::GetImpl(bool stub) {
  if (stub) {
    return new InputMethodLibraryStubImpl();
  } else {
    InputMethodLibraryImpl* impl = new InputMethodLibraryImpl();
    if (!impl->Init()) {
      LOG(ERROR) << "Failed to initialize InputMethodLibraryImpl";
    }
    return impl;
  }
}

}  // namespace chromeos

// Allows InvokeLater without adding refcounting. This class is a Singleton and
// won't be deleted until it's last InvokeLater is run.
DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::InputMethodLibraryImpl);
