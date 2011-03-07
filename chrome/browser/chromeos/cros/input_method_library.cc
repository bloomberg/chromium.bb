// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/input_method_library.h"

#include <glib.h>
#include <signal.h>

#include "unicode/uloc.h"

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/keyboard_library.h"
#include "chrome/browser/chromeos/input_method/candidate_window.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_service.h"

namespace {

const char kIBusDaemonPath[] = "/usr/bin/ibus-daemon";

// Finds a property which has |new_prop.key| from |prop_list|, and replaces the
// property with |new_prop|. Returns true if such a property is found.
bool FindAndUpdateProperty(const chromeos::ImeProperty& new_prop,
                           chromeos::ImePropertyList* prop_list) {
  for (size_t i = 0; i < prop_list->size(); ++i) {
    chromeos::ImeProperty& prop = prop_list->at(i);
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
                               public NotificationObserver {
 public:
  InputMethodLibraryImpl()
      : input_method_status_connection_(NULL),
        previous_input_method_("", "", "", ""),
        current_input_method_("", "", "", ""),
        should_launch_ime_(false),
        ime_connected_(false),
        defer_ime_startup_(false),
        enable_auto_ime_shutdown_(true),
        should_change_input_method_(false),
        ibus_daemon_process_id_(0),
        initialized_successfully_(false),
        candidate_window_controller_(NULL) {
    // Here, we use the fallback input method descriptor but
    // |current_input_method_| will be updated as soon as the login screen
    // is shown or the user is logged in, so there is no problem.
    current_input_method_ = input_method::GetFallbackInputMethodDescriptor();
    active_input_method_ids_.push_back(current_input_method_.id);
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

   if (!CrosLibrary::Get()->EnsureLoaded())
     return false;
   input_method_status_connection_ = chromeos::MonitorInputMethodStatus(
       this,
       &InputMethodChangedHandler,
       &RegisterPropertiesHandler,
       &UpdatePropertyHandler,
       &ConnectionChangeHandler);
   if (!input_method_status_connection_)
     return false;

   initialized_successfully_ = true;
   return true;
  }

  virtual ~InputMethodLibraryImpl() {
  }

  virtual void AddObserver(Observer* observer) {
    if (!observers_.size()) {
      observer->FirstObserverIsAdded(this);
    }
    observers_.AddObserver(observer);
  }

  virtual void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  virtual InputMethodDescriptors* GetActiveInputMethods() {
    chromeos::InputMethodDescriptors* result =
        new chromeos::InputMethodDescriptors;
    // Build the active input method descriptors from the active input
    // methods cache |active_input_method_ids_|.
    for (size_t i = 0; i < active_input_method_ids_.size(); ++i) {
      const std::string& input_method_id = active_input_method_ids_[i];
      const InputMethodDescriptor* descriptor =
          chromeos::input_method::GetInputMethodDescriptorFromId(
              input_method_id);
      if (descriptor) {
        result->push_back(*descriptor);
      } else {
        LOG(ERROR) << "Descriptor is not found for: " << input_method_id;
      }
    }
    // This shouldn't happen as there should be at least one active input
    // method, but just in case.
    if (result->empty()) {
      LOG(ERROR) << "No active input methods found.";
      result->push_back(input_method::GetFallbackInputMethodDescriptor());
    }
    return result;
  }

  virtual size_t GetNumActiveInputMethods() {
    scoped_ptr<InputMethodDescriptors> input_methods(GetActiveInputMethods());
    return input_methods->size();
  }

  virtual InputMethodDescriptors* GetSupportedInputMethods() {
    if (!initialized_successfully_) {
      // If initialization was failed, return the fallback input method,
      // as this function is guaranteed to return at least one descriptor.
      InputMethodDescriptors* result = new InputMethodDescriptors;
      result->push_back(input_method::GetFallbackInputMethodDescriptor());
      return result;
    }

    // This never returns NULL.
    return chromeos::GetSupportedInputMethodDescriptors();
  }

  virtual void ChangeInputMethod(const std::string& input_method_id) {
    // Changing the input method isn't guaranteed to succeed here, but we
    // should remember the last one regardless. See comments in
    // FlushImeConfig() for details.
    tentative_current_input_method_id_ = input_method_id;
    // If the input method daemon is not running and the specified input
    // method is a keyboard layout, switch the keyboard directly.
    if (ibus_daemon_process_id_ == 0 &&
        chromeos::input_method::IsKeyboardLayout(input_method_id)) {
      // We shouldn't use SetCurrentKeyboardLayoutByName() here. See
      // comments at ChangeCurrentInputMethod() for details.
      ChangeCurrentInputMethodFromId(input_method_id);
    } else {
      // Otherwise, start the input method daemon, and change the input
      // method via the damon.
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
    chromeos::SetImePropertyActivated(
        input_method_status_connection_, key.c_str(), activated);
  }

  virtual bool InputMethodIsActivated(const std::string& input_method_id) {
    scoped_ptr<InputMethodDescriptors> active_input_method_descriptors(
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
                            const ImeConfigValue& value) {
    // If the config change is for preload engines, update the active
    // input methods cache |active_input_method_ids_| here. We need to
    // update the cache before actually flushing the config. since we need
    // to return active input methods from GetActiveInputMethods() before
    // the input method daemon starts. For instance, we need to show the
    // list of available input methods (keyboard layouts) on the login
    // screen before the input method starts.
    if (section == language_prefs::kGeneralSectionName &&
        config_name == language_prefs::kPreloadEnginesConfigName &&
        value.type == ImeConfigValue::kValueTypeStringList) {
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

  virtual const InputMethodDescriptor& previous_input_method() const {
    return previous_input_method_;
  }

  virtual const InputMethodDescriptor& current_input_method() const {
    return current_input_method_;
  }

  virtual const ImePropertyList& current_ime_properties() const {
    return current_ime_properties_;
  }

  virtual std::string GetKeyboardOverlayId(const std::string& input_method_id) {
    if (!initialized_successfully_)
      return "";

    return chromeos::GetKeyboardOverlayId(input_method_id);
  }

 private:
  // Returns true if the given input method config value is a single
  // element string list that contains an input method ID of a keyboard
  // layout.
  bool ContainOnlyOneKeyboardLayout(
      const ImeConfigValue& value) {
    return (value.type == ImeConfigValue::kValueTypeStringList &&
            value.string_list_value.size() == 1 &&
            chromeos::input_method::IsKeyboardLayout(
                value.string_list_value[0]));
  }

  // Starts input method daemon based on the |defer_ime_startup_| flag and
  // input method configuration being updated. |section| is a section name of
  // the input method configuration (e.g. "general", "general/hotkey").
  // |config_name| is a name of the configuration (e.g. "preload_engines",
  // "previous_engine"). |value| is the configuration value to be set.
  void MaybeStartInputMethodDaemon(const std::string& section,
                                   const std::string& config_name,
                                   const ImeConfigValue& value) {
    if (section == language_prefs::kGeneralSectionName &&
        config_name == language_prefs::kPreloadEnginesConfigName) {
      // If there is only one input method which is a keyboard layout,
      // we don't start the input method processes.  When
      // |defer_ime_startup_| is true, we don't start it either.
      if (ContainOnlyOneKeyboardLayout(value) ||
          defer_ime_startup_) {
        // Do not start the input method daemon.
      } else {
        // Otherwise, start the input method daemon.
        StartInputMethodDaemon();
      }
    }
  }

  // Stops input method daemon based on the |enable_auto_ime_shutdown_| flag
  // and input method configuration being updated.
  // See also: MaybeStartInputMethodDaemon().
  void MaybeStopInputMethodDaemon(const std::string& section,
                                  const std::string& config_name,
                                  const ImeConfigValue& value) {
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
  void MaybeChangeCurrentKeyboardLayout(const std::string& section,
                                        const std::string& config_name,
                                        const ImeConfigValue& value) {

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
      scoped_ptr<InputMethodDescriptors> input_methods(GetActiveInputMethods());
      DCHECK(!input_methods->empty());
      if (!input_methods->empty()) {
        input_method_id_to_switch = input_methods->at(0).id;
        LOG(INFO) << "Can't change the current input method to "
                  << input_method_id << " since the engine is not preloaded. "
                  << "Switch to " << input_method_id_to_switch << " instead.";
      }
    }

    if (chromeos::ChangeInputMethod(input_method_status_connection_,
                                    input_method_id_to_switch.c_str())) {
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
      const ImeConfigValue& value = iter->second;
      if (chromeos::SetImeConfig(input_method_status_connection_,
                                 section.c_str(),
                                 config_name.c_str(),
                                 value)) {
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
    if (pending_config_requests_.empty()) {
      // We should change the current input method to the one we have last
      // remembered in ChangeInputMethod(), for the following reasons:
      //
      // 1) Calls to ChangeInputMethod() will fail if the input method has not
      // yet been added to preload_engines.  As such, the call is deferred
      // until after all config values have been sent to the IME process.
      //
      // 2) We might have already changed the current input method to one
      // of XKB layouts without going through the IBus daemon (we can do
      // it without the IBus daemon started).
      if (should_change_input_method_ &&
          !tentative_current_input_method_id_.empty()) {
        ChangeInputMethodViaIBus(tentative_current_input_method_id_);
        tentative_current_input_method_id_ = "";
        should_change_input_method_ = false;
        active_input_methods_are_changed = true;
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
      FOR_EACH_OBSERVER(Observer, observers_,
                        ActiveInputMethodsChanged(this,
                                                  current_input_method_,
                                                  num_active_input_methods));
    }
  }

  // Called when the input method is changed in the IBus daemon
  // (ex. "global-engine-changed" is delivered from the IBus daemon).
  static void InputMethodChangedHandler(
      void* object,
      const chromeos::InputMethodDescriptor& current_input_method) {
    // The handler is called when the input method method change is
    // notified via a DBus connection. Since the DBus notificatiosn are
    // handled in the UI thread, we can assume that this function always
    // runs on the UI thread, but just in case.
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      LOG(ERROR) << "Not on UI thread";
      return;
    }

    InputMethodLibraryImpl* input_method_library =
        static_cast<InputMethodLibraryImpl*>(object);
    input_method_library->ChangeCurrentInputMethod(current_input_method);
  }

  // Called when properties are registered in the IBus daemon.
  static void RegisterPropertiesHandler(
      void* object, const ImePropertyList& prop_list) {
    // See comments in InputMethodChangedHandler.
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      LOG(ERROR) << "Not on UI thread";
      return;
    }

    InputMethodLibraryImpl* input_method_library =
        static_cast<InputMethodLibraryImpl*>(object);
    input_method_library->RegisterProperties(prop_list);
  }

  // Called when properties are updated in the IBus daemon.
  static void UpdatePropertyHandler(
      void* object, const ImePropertyList& prop_list) {
    // See comments in InputMethodChangedHandler.
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      LOG(ERROR) << "Not on UI thread";
      return;
    }

    InputMethodLibraryImpl* input_method_library =
        static_cast<InputMethodLibraryImpl*>(object);
    input_method_library->UpdateProperty(prop_list);
  }

  // Called when 1) connection to ibus-daemon and ibus-memconf are established
  // or 2) connection to ibus-daemon is terminated.
  static void ConnectionChangeHandler(void* object, bool connected) {
    // See comments in InputMethodChangedHandler.
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      LOG(ERROR) << "Not on UI thread";
      return;
    }

    InputMethodLibraryImpl* input_method_library =
        static_cast<InputMethodLibraryImpl*>(object);
    input_method_library->ime_connected_ = connected;
    if (connected) {
      input_method_library->pending_config_requests_.clear();
      input_method_library->pending_config_requests_.insert(
          input_method_library->current_config_values_.begin(),
          input_method_library->current_config_values_.end());
      input_method_library->should_change_input_method_ = true;
      input_method_library->FlushImeConfig();
    }
  }

  // Changes the current input method from the given input method
  // descriptor.  This function updates states like current_input_method_
  // and notifies observers about the change (that will update the
  // preferences), hence this function should always be used even if you
  // just need to change the current keyboard layout.
  void ChangeCurrentInputMethod(const InputMethodDescriptor& new_input_method) {
    if (current_input_method_.id != new_input_method.id) {
      previous_input_method_ = current_input_method_;
      current_input_method_ = new_input_method;

      // Change the keyboard layout to a preferred layout for the input method.
      if (!CrosLibrary::Get()->GetKeyboardLibrary()->
          SetCurrentKeyboardLayoutByName(
              current_input_method_.keyboard_layout)) {
        LOG(ERROR) << "Failed to change keyboard layout to "
                   << current_input_method_.keyboard_layout;
      }

      // Ask the first observer to update preferences. We should not ask every
      // observer to do so. Otherwise, we'll end up updating preferences many
      // times when many observers are attached (ex. many windows are opened),
      // which is unnecessary and expensive.
      ObserverListBase<Observer>::Iterator it(observers_);
      Observer* first_observer = it.GetNext();
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
    FOR_EACH_OBSERVER(Observer, observers_,
                      InputMethodChanged(this,
                                         current_input_method_,
                                         num_active_input_methods));
  }

  // Changes the current input method from the given input method ID.
  // This function is just a wrapper of ChangeCurrentInputMethod().
  void ChangeCurrentInputMethodFromId(const std::string& input_method_id) {
    const chromeos::InputMethodDescriptor* descriptor =
        chromeos::input_method::GetInputMethodDescriptorFromId(
            input_method_id);
    if (descriptor) {
      ChangeCurrentInputMethod(*descriptor);
    } else {
      LOG(ERROR) << "Descriptor is not found for: " << input_method_id;
    }
  }

  // Registers the properties used by the current input method.
  void RegisterProperties(const ImePropertyList& prop_list) {
    // |prop_list| might be empty. This means "clear all properties."
    current_ime_properties_ = prop_list;
  }

  // Starts the input method daemon. Unlike MaybeStopInputMethodDaemon(),
  // this function always starts the daemon.
  void StartInputMethodDaemon() {
    should_launch_ime_ = true;
    MaybeLaunchInputMethodDaemon();
  }

  // Updates the properties used by the current input method.
  void UpdateProperty(const ImePropertyList& prop_list) {
    for (size_t i = 0; i < prop_list.size(); ++i) {
      FindAndUpdateProperty(prop_list[i], &current_ime_properties_);
    }
  }

  // Launches an input method procsess specified by the given command
  // line. On success, returns true and stores the process ID in
  // |process_id|. Otherwise, returns false, and the contents of
  // |process_id| is untouched. OnImeShutdown will be called when the
  // process terminates.
  bool LaunchInputMethodProcess(const std::string& command_line,
                                int* process_id) {
    GError *error = NULL;
    gchar **argv = NULL;
    gint argc = NULL;
    // TODO(zork): export "LD_PRELOAD=/usr/lib/libcrash.so"
    if (!g_shell_parse_argv(command_line.c_str(), &argc, &argv, &error)) {
      LOG(ERROR) << "Could not parse command: " << error->message;
      g_error_free(error);
      return false;
    }

    int pid = 0;
    const GSpawnFlags kFlags = G_SPAWN_DO_NOT_REAP_CHILD;
    const gboolean result = g_spawn_async(NULL, argv, NULL,
                                          kFlags, NULL, NULL,
                                          &pid, &error);
    g_strfreev(argv);
    if (!result) {
      LOG(ERROR) << "Could not launch: " << command_line << ": "
                 << error->message;
      g_error_free(error);
      return false;
    }
    g_child_watch_add(pid, reinterpret_cast<GChildWatchFunc>(OnImeShutdown),
                      this);

    *process_id = pid;
    VLOG(1) << command_line << " (PID=" << pid << ") is started";
    return  true;
  }

  // Launches input method daemon if these are not yet running.
  void MaybeLaunchInputMethodDaemon() {
    // CandidateWindowController requires libcros to be loaded. Besides,
    // launching ibus-daemon without libcros loaded doesn't make sense.
    if (!initialized_successfully_)
      return;

    if (!should_launch_ime_) {
      return;
    }

    if (!candidate_window_controller_.get()) {
      candidate_window_controller_.reset(new CandidateWindowController);
      if (!candidate_window_controller_->Init()) {
        LOG(WARNING) << "Failed to initialize the candidate window controller";
      }
    }

    if (ibus_daemon_process_id_ == 0) {
      // TODO(zork): Send output to /var/log/ibus.log
      const std::string ibus_daemon_command_line =
          StringPrintf("%s --panel=disable --cache=none --restart --replace",
                       kIBusDaemonPath);
      if (!LaunchInputMethodProcess(
              ibus_daemon_command_line, &ibus_daemon_process_id_)) {
        LOG(ERROR) << "Failed to launch " << ibus_daemon_command_line;
      }
    }
  }

  // Called when the input method process is shut down.
  static void OnImeShutdown(int pid,
                            int status,
                            InputMethodLibraryImpl* library) {
    g_spawn_close_pid(pid);
    if (library->ibus_daemon_process_id_ == pid) {
      library->ibus_daemon_process_id_ = 0;
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
    if (ibus_daemon_process_id_) {
      if (!chromeos::StopInputMethodProcess(input_method_status_connection_)) {
        LOG(ERROR) << "StopInputMethodProcess IPC failed. Sending SIGTERM to "
                   << "PID " << ibus_daemon_process_id_;
        kill(ibus_daemon_process_id_, SIGTERM);
      }
      VLOG(1) << "ibus-daemon (PID=" << ibus_daemon_process_id_ << ") is "
              << "terminated";
      ibus_daemon_process_id_ = 0;
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
    }
  }

  // A reference to the language api, to allow callbacks when the input method
  // status changes.
  InputMethodStatusConnection* input_method_status_connection_;
  ObserverList<Observer> observers_;

  // The input method which was/is selected.
  InputMethodDescriptor previous_input_method_;
  InputMethodDescriptor current_input_method_;

  // The input method properties which the current input method uses. The list
  // might be empty when no input method is used.
  ImePropertyList current_ime_properties_;

  typedef std::pair<std::string, std::string> ConfigKeyType;
  typedef std::map<ConfigKeyType, ImeConfigValue> InputMethodConfigRequests;
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
  std::string tentative_current_input_method_id_;
  // True if we should change the current input method to
  // |tentative_current_input_method_id_| once the queue of the pending config
  // requests becomes empty.
  bool should_change_input_method_;

  // The process id of the IBus daemon. 0 if it's not running. The process
  // ID 0 is not used in Linux, hence it's safe to use 0 for this purpose.
  int ibus_daemon_process_id_;

  // True if initialization is successfully done, meaning that libcros is
  // loaded and input method status monitoring is started. This value
  // should be checked where we call libcros functions.
  bool initialized_successfully_;

  // The candidate window.
  scoped_ptr<CandidateWindowController> candidate_window_controller_;

  // The active input method ids cache.
  std::vector<std::string> active_input_method_ids_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodLibraryImpl);
};

InputMethodLibraryImpl::Observer::~Observer() {}

// The stub implementation of InputMethodLibrary. Used for testing.
class InputMethodLibraryStubImpl : public InputMethodLibrary {
 public:
  InputMethodLibraryStubImpl()
      : previous_input_method_("", "", "", ""),
        current_input_method_("", "", "", ""),
        keyboard_overlay_map_(
            GetKeyboardOverlayMapForTesting()) {
    current_input_method_ = input_method::GetFallbackInputMethodDescriptor();
  }

  virtual ~InputMethodLibraryStubImpl() {}
  virtual void AddObserver(Observer* observer) {}
  virtual void RemoveObserver(Observer* observer) {}

  virtual InputMethodDescriptors* GetActiveInputMethods() {
    return GetInputMethodDescriptorsForTesting();
  }


  virtual size_t GetNumActiveInputMethods() {
    scoped_ptr<InputMethodDescriptors> descriptors(GetActiveInputMethods());
    return descriptors->size();
  }

  virtual InputMethodDescriptors* GetSupportedInputMethods() {
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
                            const ImeConfigValue& value) {
    return false;
  }

  virtual const InputMethodDescriptor& previous_input_method() const {
    return previous_input_method_;
  }

  virtual const InputMethodDescriptor& current_input_method() const {
    return current_input_method_;
  }

  virtual const ImePropertyList& current_ime_properties() const {
    return current_ime_properties_;
  }

  virtual void StartInputMethodDaemon() {}
  virtual void StopInputMethodDaemon() {}
  virtual void SetDeferImeStartup(bool defer) {}
  virtual void SetEnableAutoImeShutdown(bool enable) {}

  virtual std::string GetKeyboardOverlayId(const std::string& input_method_id) {
    KeyboardOverlayMap::const_iterator iter =
        keyboard_overlay_map_->find(input_method_id);
    return (iter != keyboard_overlay_map_->end()) ?
        iter->second : "";
  }

 private:
  typedef std::map<std::string, std::string> KeyboardOverlayMap;

  // Gets input method descriptors for testing. Shouldn't be used for
  // production.
  InputMethodDescriptors* GetInputMethodDescriptorsForTesting() {
    InputMethodDescriptors* descriptions = new InputMethodDescriptors;
    // The list is created from output of gen_engines.py in libcros.
    // % SHARE=/build/x86-generic/usr/share python gen_engines.py
    // $SHARE/chromeos-assets/input_methods/whitelist.txt
    // $SHARE/ibus/component/{chewing,hangul,m17n,mozc,pinyin,xkb-layouts}.xml
    descriptions->push_back(InputMethodDescriptor(
        "xkb:nl::nld", "Netherlands", "nl", "nld"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:be::nld", "Belgium", "be", "nld"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:fr::fra", "France", "fr", "fra"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:be::fra", "Belgium", "be", "fra"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:ca::fra", "Canada", "ca", "fra"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:ch:fr:fra", "Switzerland - French", "ch(fr)", "fra"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:de::ger", "Germany", "de", "ger"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:de:neo:ger", "Germany - Neo 2", "de(neo)", "ger"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:be::ger", "Belgium", "be", "ger"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:ch::ger", "Switzerland", "ch", "ger"));
    descriptions->push_back(InputMethodDescriptor(
        "mozc", "Mozc (US keyboard layout)", "us", "ja"));
    descriptions->push_back(InputMethodDescriptor(
        "mozc-jp", "Mozc (Japanese keyboard layout)", "jp", "ja"));
    descriptions->push_back(InputMethodDescriptor(
        "mozc-dv", "Mozc (US Dvorak keyboard layout)", "us(dvorak)", "ja"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:jp::jpn", "Japan", "jp", "jpn"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:ru::rus", "Russia", "ru", "rus"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:ru:phonetic:rus", "Russia - Phonetic", "ru(phonetic)", "rus"));
    descriptions->push_back(InputMethodDescriptor(
        "m17n:th:kesmanee", "kesmanee (m17n)", "us", "th"));
    descriptions->push_back(InputMethodDescriptor(
        "m17n:th:pattachote", "pattachote (m17n)", "us", "th"));
    descriptions->push_back(InputMethodDescriptor(
        "m17n:th:tis820", "tis820 (m17n)", "us", "th"));
    descriptions->push_back(InputMethodDescriptor(
        "chewing", "Chewing", "us", "zh_TW"));
    descriptions->push_back(InputMethodDescriptor(
        "m17n:zh:cangjie", "cangjie (m17n)", "us", "zh"));
    descriptions->push_back(InputMethodDescriptor(
        "m17n:zh:quick", "quick (m17n)", "us", "zh"));
    descriptions->push_back(InputMethodDescriptor(
        "m17n:vi:tcvn", "tcvn (m17n)", "us", "vi"));
    descriptions->push_back(InputMethodDescriptor(
        "m17n:vi:telex", "telex (m17n)", "us", "vi"));
    descriptions->push_back(InputMethodDescriptor(
        "m17n:vi:viqr", "viqr (m17n)", "us", "vi"));
    descriptions->push_back(InputMethodDescriptor(
        "m17n:vi:vni", "vni (m17n)", "us", "vi"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:us::eng", "USA", "us", "eng"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:us:intl:eng",
        "USA - International (with dead keys)", "us(intl)", "eng"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:us:altgr-intl:eng",
        "USA - International (AltGr dead keys)", "us(altgr-intl)", "eng"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:us:dvorak:eng", "USA - Dvorak", "us(dvorak)", "eng"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:us:colemak:eng", "USA - Colemak", "us(colemak)", "eng"));
    descriptions->push_back(InputMethodDescriptor(
        "hangul", "Korean", "kr(kr104)", "ko"));
    descriptions->push_back(InputMethodDescriptor(
        "pinyin", "Pinyin", "us", "zh"));
    descriptions->push_back(InputMethodDescriptor(
        "m17n:ar:kbd", "kbd (m17n)", "us", "ar"));
    descriptions->push_back(InputMethodDescriptor(
        "m17n:hi:itrans", "itrans (m17n)", "us", "hi"));
    descriptions->push_back(InputMethodDescriptor(
        "m17n:fa:isiri", "isiri (m17n)", "us", "fa"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:br::por", "Brazil", "br", "por"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:bg::bul", "Bulgaria", "bg", "bul"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:bg:phonetic:bul",
        "Bulgaria - Traditional phonetic", "bg(phonetic)", "bul"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:ca:eng:eng", "Canada - English", "ca(eng)", "eng"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:cz::cze", "Czechia", "cz", "cze"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:ee::est", "Estonia", "ee", "est"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:es::spa", "Spain", "es", "spa"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:es:cat:cat",
        "Spain - Catalan variant with middle-dot L", "es(cat)", "cat"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:dk::dan", "Denmark", "dk", "dan"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:gr::gre", "Greece", "gr", "gre"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:il::heb", "Israel", "il", "heb"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:kr:kr104:kor",
        "Korea, Republic of - 101/104 key Compatible", "kr(kr104)", "kor"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:latam::spa", "Latin American", "latam", "spa"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:lt::lit", "Lithuania", "lt", "lit"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:lv:apostrophe:lav",
        "Latvia - Apostrophe (') variant", "lv(apostrophe)", "lav"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:hr::scr", "Croatia", "hr", "scr"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:gb:extd:eng",
        "United Kingdom - Extended - Winkeys", "gb(extd)", "eng"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:fi::fin", "Finland", "fi", "fin"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:hu::hun", "Hungary", "hu", "hun"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:it::ita", "Italy", "it", "ita"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:no::nob", "Norway", "no", "nob"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:pl::pol", "Poland", "pl", "pol"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:pt::por", "Portugal", "pt", "por"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:ro::rum", "Romania", "ro", "rum"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:se::swe", "Sweden", "se", "swe"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:sk::slo", "Slovakia", "sk", "slo"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:si::slv", "Slovenia", "si", "slv"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:rs::srp", "Serbia", "rs", "srp"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:tr::tur", "Turkey", "tr", "tur"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:ua::ukr", "Ukraine", "ua", "ukr"));
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
    (*keyboard_overlay_map)["chewing"] = "zh_TW";
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

  InputMethodDescriptor previous_input_method_;
  InputMethodDescriptor current_input_method_;
  ImePropertyList current_ime_properties_;
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
