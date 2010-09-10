// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/keyboard_library.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/language_preferences.h"

namespace {
const char kIBusDaemonPath[] = "/usr/bin/ibus-daemon";
const char kCandidateWindowPath[] = "/opt/google/chrome/candidate_window";

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

class InputMethodLibraryImpl : public InputMethodLibrary {
 public:
  InputMethodLibraryImpl()
      : input_method_status_connection_(NULL),
        previous_input_method_("", "", "", ""),
        current_input_method_("", "", "", ""),
        should_launch_ime_(false),
        ime_connected_(false),
        defer_ime_startup_(false),
        should_change_input_method_(false),
        ibus_daemon_process_id_(0),
        candidate_window_process_id_(0),
        failure_count_(0) {
    scoped_ptr<InputMethodDescriptors> input_method_descriptors(
        CreateFallbackInputMethodDescriptors());
    current_input_method_ = input_method_descriptors->at(0);
    if (CrosLibrary::Get()->EnsureLoaded()) {
      current_input_method_id_ = chromeos::GetHardwareKeyboardLayoutName();
    }
  }

  ~InputMethodLibraryImpl() {
    StopInputMethodProcesses();
  }

  void AddObserver(Observer* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  InputMethodDescriptors* GetActiveInputMethods() {
    chromeos::InputMethodDescriptors* result = NULL;
    // The connection does not need to be alive, but it does need to be created.
    if (EnsureLoadedAndStarted()) {
      result = chromeos::GetActiveInputMethods(input_method_status_connection_);
    }
    if (!result || result->empty()) {
      result = CreateFallbackInputMethodDescriptors();
    }
    return result;
  }

  size_t GetNumActiveInputMethods() {
    scoped_ptr<InputMethodDescriptors> input_methods(GetActiveInputMethods());
    return input_methods->size();
  }

  InputMethodDescriptors* GetSupportedInputMethods() {
    InputMethodDescriptors* result = NULL;
    // The connection does not need to be alive, but it does need to be created.
    if (EnsureLoadedAndStarted()) {
      result = chromeos::GetSupportedInputMethods(
          input_method_status_connection_);
    }
    if (!result || result->empty()) {
      result = CreateFallbackInputMethodDescriptors();
    }
    return result;
  }

  void ChangeInputMethod(const std::string& input_method_id) {
    current_input_method_id_ = input_method_id;
    if (EnsureLoadedAndStarted()) {
      if (input_method_id != chromeos::GetHardwareKeyboardLayoutName()) {
        StartInputMethodProcesses();
      }
      chromeos::ChangeInputMethod(
          input_method_status_connection_, input_method_id.c_str());
    }
  }

  void SetImePropertyActivated(const std::string& key, bool activated) {
    DCHECK(!key.empty());
    if (EnsureLoadedAndStarted()) {
      chromeos::SetImePropertyActivated(
          input_method_status_connection_, key.c_str(), activated);
    }
  }

  bool InputMethodIsActivated(const std::string& input_method_id) {
    scoped_ptr<InputMethodDescriptors> active_input_method_descriptors(
        CrosLibrary::Get()->GetInputMethodLibrary()->GetActiveInputMethods());
    for (size_t i = 0; i < active_input_method_descriptors->size(); ++i) {
      if (active_input_method_descriptors->at(i).id == input_method_id) {
        return true;
      }
    }
    return false;
  }

  bool GetImeConfig(const char* section, const char* config_name,
                    ImeConfigValue* out_value) {
    bool success = false;
    if (EnsureLoadedAndStarted()) {
      success = chromeos::GetImeConfig(input_method_status_connection_,
                                       section, config_name, out_value);
    }
    return success;
  }

  bool SetImeConfig(const char* section, const char* config_name,
                    const ImeConfigValue& value) {
    MaybeStartOrStopInputMethodProcesses(section, config_name, value);

    const ConfigKeyType key = std::make_pair(section, config_name);
    current_config_values_[key] = value;
    if (ime_connected_) {
      pending_config_requests_[key] = value;
      FlushImeConfig();
    }
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

 private:
  // Starts or stops the input method processes based on the current state.
  void MaybeStartOrStopInputMethodProcesses(
      const char* section,
      const char* config_name,
      const ImeConfigValue& value) {
    if (!strcmp(language_prefs::kGeneralSectionName, section) &&
        !strcmp(language_prefs::kPreloadEnginesConfigName, config_name)) {
      if (EnsureLoadedAndStarted()) {
        // If there are no input methods other than one for the hardware
        // keyboard, we'll stop the input method processes.
        if (value.type == ImeConfigValue::kValueTypeStringList &&
            value.string_list_value.size() == 1 &&
            value.string_list_value[0] ==
            chromeos::GetHardwareKeyboardLayoutName()) {
          StopInputMethodProcesses();
        } else if (!defer_ime_startup_) {
          StartInputMethodProcesses();
        }
        chromeos::SetActiveInputMethods(input_method_status_connection_, value);
      }
    }
  }

  // Flushes the input method config data. The config data is queued up in
  // |pending_config_requests_| until the config backend (ibus-memconf)
  // starts. Since there is no good way to get notified when the config
  // backend starts, we use a timer to periodically attempt to send the
  // config data to the config backend.
  void FlushImeConfig() {
    bool active_input_methods_are_changed = false;
    bool completed = false;
    if (EnsureLoadedAndStarted()) {
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
          }
          // Successfully sent. Remove the command and proceed to the next one.
          pending_config_requests_.erase(iter++);
        } else {
          // If SetImeConfig() fails, subsequent calls will likely fail.
          break;
        }
      }
      if (pending_config_requests_.empty()) {
        // Calls to ChangeInputMethod() will fail if the input method has not
        // yet been added to preload_engines.  As such, the call is deferred
        // until after all config values have been sent to the IME process.
        if (should_change_input_method_) {
          if (chromeos::ChangeInputMethod(input_method_status_connection_,
                                          current_input_method_id_.c_str())) {
            should_change_input_method_ = false;
            completed = true;
            active_input_methods_are_changed = true;
          }
        } else {
          completed = true;
        }
      }
    }

    if (completed) {
      timer_.Stop();  // no-op if it's not running.
    } else {
      // Flush is not completed. Start a timer if it's not yet running.
      if (!timer_.IsRunning()) {
        static const int64 kTimerIntervalInMsec = 100;
        failure_count_ = 0;
        timer_.Start(base::TimeDelta::FromMilliseconds(kTimerIntervalInMsec),
                     this, &InputMethodLibraryImpl::FlushImeConfig);
      } else {
        // The timer is already running. We'll give up if it reaches the
        // max retry count.
        static const int kMaxRetries = 15;
        ++failure_count_;
        if (failure_count_ > kMaxRetries) {
          LOG(ERROR) << "FlushImeConfig: Max retries exceeded. "
                     << "current_input_method_id: " << current_input_method_id_
                     << " pending_config_requests.size: "
                     << pending_config_requests_.size();
          timer_.Stop();
        }
      }
    }
    if (active_input_methods_are_changed) {
      FOR_EACH_OBSERVER(Observer, observers_, ActiveInputMethodsChanged(this));
    }
  }

  static void InputMethodChangedHandler(
      void* object,
      const chromeos::InputMethodDescriptor& current_input_method) {
    // The handler is called when the input method method change is
    // notified via a DBus connection. Since the DBus notificatiosn are
    // handled in the UI thread, we can assume that this functionalways
    // runs on the UI thread, but just in case.
    if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
      LOG(ERROR) << "Not on UI thread";
      return;
    }

    InputMethodLibraryImpl* input_method_library =
        static_cast<InputMethodLibraryImpl*>(object);
    input_method_library->ChangeCurrentInputMethod(current_input_method);
  }

  static void RegisterPropertiesHandler(
      void* object, const ImePropertyList& prop_list) {
    // See comments in InputMethodChangedHandler.
    if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
      LOG(ERROR) << "Not on UI thread";
      return;
    }

    InputMethodLibraryImpl* input_method_library =
        static_cast<InputMethodLibraryImpl*>(object);
    input_method_library->RegisterProperties(prop_list);
  }

  static void UpdatePropertyHandler(
      void* object, const ImePropertyList& prop_list) {
    // See comments in InputMethodChangedHandler.
    if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
      LOG(ERROR) << "Not on UI thread";
      return;
    }

    InputMethodLibraryImpl* input_method_library =
        static_cast<InputMethodLibraryImpl*>(object);
    input_method_library->UpdateProperty(prop_list);
  }

  static void ConnectionChangeHandler(void* object, bool connected) {
    // See comments in InputMethodChangedHandler.
    if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
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
    } else {
      // Stop attempting to resend config data, since it will continue to fail.
      input_method_library->timer_.Stop();  // no-op if it's not running.
    }
  }

  bool EnsureStarted() {
    if (!input_method_status_connection_) {
      input_method_status_connection_ = chromeos::MonitorInputMethodStatus(
          this,
          &InputMethodChangedHandler,
          &RegisterPropertiesHandler,
          &UpdatePropertyHandler,
          &ConnectionChangeHandler);
    }
    return true;
  }

  bool EnsureLoadedAndStarted() {
    return CrosLibrary::Get()->EnsureLoaded() &&
           EnsureStarted();
  }

  void ChangeCurrentInputMethod(const InputMethodDescriptor& new_input_method) {
    // Change the keyboard layout to a preferred layout for the input method.
    CrosLibrary::Get()->GetKeyboardLibrary()->SetCurrentKeyboardLayoutByName(
        new_input_method.keyboard_layout);

    if (current_input_method_.id != new_input_method.id) {
      previous_input_method_ = current_input_method_;
      current_input_method_ = new_input_method;
    }
    FOR_EACH_OBSERVER(Observer, observers_, InputMethodChanged(this));
  }

  void RegisterProperties(const ImePropertyList& prop_list) {
    // |prop_list| might be empty. This means "clear all properties."
    current_ime_properties_ = prop_list;
    FOR_EACH_OBSERVER(Observer, observers_, ImePropertiesChanged(this));
  }

  void StartInputMethodProcesses() {
    should_launch_ime_ = true;
    MaybeLaunchInputMethodProcesses();
  }

  void UpdateProperty(const ImePropertyList& prop_list) {
    for (size_t i = 0; i < prop_list.size(); ++i) {
      FindAndUpdateProperty(prop_list[i], &current_ime_properties_);
    }
    FOR_EACH_OBSERVER(Observer, observers_, ImePropertiesChanged(this));
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
    return  true;
  }

  // Launches input method processes if these are not yet running.
  void MaybeLaunchInputMethodProcesses() {
    if (!should_launch_ime_) {
      return;
    }

    if (ibus_daemon_process_id_ == 0) {
      // TODO(zork): Send output to /var/log/ibus.log
      const std::string ibus_daemon_command_line =
          StringPrintf("%s --panel=disable --cache=none --restart --replace",
                       kIBusDaemonPath);
      if (!LaunchInputMethodProcess(ibus_daemon_command_line,
                                    &ibus_daemon_process_id_)) {
        // On failure, we should not attempt to launch candidate_window.
        return;
      }
    }

    if (candidate_window_process_id_ == 0) {
      // Pass the UI language info to candidate_window via --lang flag.
      const std::string candidate_window_command_line =
          StringPrintf("%s --lang=%s", kCandidateWindowPath,
                       g_browser_process->GetApplicationLocale().c_str());
      if (!LaunchInputMethodProcess(candidate_window_command_line,
                                    &candidate_window_process_id_)) {
        // Return here just in case we add more code below.
        return;
      }
    }
  }

  static void OnImeShutdown(int pid,
                            int status,
                            InputMethodLibraryImpl* library) {
    g_spawn_close_pid(pid);
    if (library->ibus_daemon_process_id_ == pid) {
      library->ibus_daemon_process_id_ = 0;
    } else if (library->candidate_window_process_id_ == pid) {
      library->candidate_window_process_id_ = 0;
    }

    // Restart input method processes if needed.
    library->MaybeLaunchInputMethodProcesses();
  }

  void StopInputMethodProcesses() {
    should_launch_ime_ = false;
    if (ibus_daemon_process_id_) {
      const std::string xkb_engine_name =
          chromeos::GetHardwareKeyboardLayoutName();
      // We should not use chromeos::ChangeInputMethod() here since without the
      // ibus-daemon process, ChangeCurrentInputMethod() callback function which
      // actually changes the XKB layout will not be called.
      CrosLibrary::Get()->GetKeyboardLibrary()->SetCurrentKeyboardLayoutByName(
          chromeos::input_method::GetKeyboardLayoutName(xkb_engine_name));
      kill(ibus_daemon_process_id_, SIGTERM);
      ibus_daemon_process_id_ = 0;
    }
    if (candidate_window_process_id_) {
      kill(candidate_window_process_id_, SIGTERM);
      candidate_window_process_id_ = 0;
    }
  }

  void SetDeferImeStartup(bool defer) {
    LOG(INFO) << "Setting DeferImeStartup to " << defer;
    defer_ime_startup_ = defer;
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

  // A timer for retrying to send |pendning_config_commands_| to the input
  // method config daemon.
  base::OneShotTimer<InputMethodLibraryImpl> timer_;

  // True if we should launch the input method processes.
  bool should_launch_ime_;
  // True if the connection to the IBus daemon is alive.
  bool ime_connected_;
  // If true, we'll defer the startup until a non-default method is
  // activated.
  bool defer_ime_startup_;
  // The ID of the current input method (ex. "mozc").
  std::string current_input_method_id_;
  // True if we should change the input method once the queue of the
  // pending config requests becomes empty.
  bool should_change_input_method_;

  // The process id of the IBus daemon. 0 if it's not running. The process
  // ID 0 is not used in Linux, hence it's safe to use 0 for this purpose.
  int ibus_daemon_process_id_;
  // The process id of the candidate window. 0 if it's not running.
  int candidate_window_process_id_;

  int failure_count_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodLibraryImpl);
};

InputMethodLibraryImpl::Observer::~Observer() {}

class InputMethodLibraryStubImpl : public InputMethodLibrary {
 public:
  InputMethodLibraryStubImpl()
      : previous_input_method_("", "", "", ""),
        current_input_method_("", "", "", "") {
  }

  ~InputMethodLibraryStubImpl() {}
  void AddObserver(Observer* observer) {}
  void RemoveObserver(Observer* observer) {}

  InputMethodDescriptors* GetActiveInputMethods() {
    return CreateRealisticInputMethodDescriptors();
  }


  size_t GetNumActiveInputMethods() {
    scoped_ptr<InputMethodDescriptors> descriptors(
        CreateRealisticInputMethodDescriptors());
    return descriptors->size();
  }

  InputMethodDescriptors* GetSupportedInputMethods() {
    return CreateRealisticInputMethodDescriptors();
  }

  void ChangeInputMethod(const std::string& input_method_id) {}
  void SetImePropertyActivated(const std::string& key, bool activated) {}

  bool InputMethodIsActivated(const std::string& input_method_id) {
    return true;
  }

  bool GetImeConfig(const char* section,
                    const char* config_name,
                    ImeConfigValue* out_value) {
    return false;
  }

  bool SetImeConfig(const char* section,
                    const char* config_name,
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

  virtual void StartInputMethodProcesses() {}
  virtual void StopInputMethodProcesses() {}
  virtual void SetDeferImeStartup(bool defer) {}

 private:
  // Creates realistic input method descriptors that can be used for
  // testing Chrome OS version of chrome on regular Linux desktops.
  InputMethodDescriptors* CreateRealisticInputMethodDescriptors() {
    InputMethodDescriptors* descriptions = new InputMethodDescriptors;
    // The list is created from output of gen_engines.py in libcros.
    descriptions->push_back(InputMethodDescriptor(
        "chewing", "Chewing", "us", "zh_TW"));
    descriptions->push_back(InputMethodDescriptor(
        "hangul", "Korean", "us", "ko"));
    descriptions->push_back(InputMethodDescriptor(
        "m17n:fa:isiri", "isiri (m17n)", "us", "fa"));
    descriptions->push_back(InputMethodDescriptor(
        "m17n:he:kbd", "kbd (m17n)", "us", "he"));
    descriptions->push_back(InputMethodDescriptor(
        "m17n:ar:kbd", "kbd (m17n)", "us", "ar"));
    descriptions->push_back(InputMethodDescriptor(
        "m17n:hi:itrans", "itrans (m17n)", "us", "hi"));
    descriptions->push_back(InputMethodDescriptor(
        "m17n:vi:vni", "vni (m17n)", "us", "vi"));
    descriptions->push_back(InputMethodDescriptor(
        "m17n:vi:viqr", "viqr (m17n)", "us", "vi"));
    descriptions->push_back(InputMethodDescriptor(
        "m17n:vi:tcvn", "tcvn (m17n)", "us", "vi"));
    descriptions->push_back(InputMethodDescriptor(
        "m17n:vi:telex", "telex (m17n)", "us", "vi"));
    descriptions->push_back(InputMethodDescriptor(
        "m17n:zh:cangjie", "cangjie (m17n)", "us", "zh"));
    descriptions->push_back(InputMethodDescriptor(
        "m17n:zh:quick", "quick (m17n)", "us", "zh"));
    descriptions->push_back(InputMethodDescriptor(
        "m17n:th:tis820", "tis820 (m17n)", "us", "th"));
    descriptions->push_back(InputMethodDescriptor(
        "m17n:th:kesmanee", "kesmanee (m17n)", "us", "th"));
    descriptions->push_back(InputMethodDescriptor(
        "m17n:th:pattachote", "pattachote (m17n)", "us", "th"));
    descriptions->push_back(InputMethodDescriptor(
        "mozc-jp", "Mozc (Japanese keyboard layout)", "jp", "ja"));
    descriptions->push_back(InputMethodDescriptor(
        "mozc", "Mozc (US keyboard layout)", "us", "ja"));
    descriptions->push_back(InputMethodDescriptor(
        "mozc-dv", "Mozc (US Dvorak keyboard layout)", "us(dvorak)", "ja"));
    descriptions->push_back(InputMethodDescriptor(
        "pinyin", "Pinyin", "us", "zh"));
    descriptions->push_back(InputMethodDescriptor(
        "bopomofo", "Bopomofo", "us", "zh"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:us::eng", "USA", "us", "eng"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:us:dvorak:eng", "USA - Dvorak", "us(dvorak)", "eng"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:be::ger", "Belgium", "be", "ger"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:be::nld", "Belgium", "be", "nld"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:be::fra", "Belgium", "be", "fra"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:br::por", "Brazil", "br", "por"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:bg::bul", "Bulgaria", "bg", "bul"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:ca::fra", "Canada", "ca", "fra"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:ca:eng:eng", "Canada - English", "ca(eng)", "eng"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:hr::scr", "Croatia", "hr", "scr"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:cz::cze", "Czechia", "cz", "cze"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:dk::dan", "Denmark", "dk", "dan"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:nl::nld", "Netherlands", "nl", "nld"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:ee::est", "Estonia", "ee", "est"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:fi::fin", "Finland", "fi", "fin"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:fr::fra", "France", "fr", "fra"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:de::ger", "Germany", "de", "ger"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:gr::gre", "Greece", "gr", "gre"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:hu::hun", "Hungary", "hu", "hun"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:it::ita", "Italy", "it", "ita"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:jp::jpn", "Japan", "jp", "jpn"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:lt::lit", "Lithuania", "lt", "lit"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:lv::lav", "Latvia", "lv", "lav"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:no::nor", "Norway", "no", "nor"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:pl::pol", "Poland", "pl", "pol"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:pt::por", "Portugal", "pt", "por"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:ro::rum", "Romania", "ro", "rum"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:ru::rus", "Russia", "ru", "rus"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:rs::srp", "Serbia", "rs", "srp"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:si::slv", "Slovenia", "si", "slv"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:sk::slo", "Slovakia", "sk", "slo"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:es::spa", "Spain", "es", "spa"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:es:cat:cat",
        "Spain - Catalan variant with middle-dot L", "es(cat)", "cat"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:se::swe", "Sweden", "se", "swe"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:ch::ger", "Switzerland", "ch", "ger"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:ch:fr:fra", "Switzerland - French", "ch(fr)", "fra"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:tr::tur", "Turkey", "tr", "tur"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:ua::ukr", "Ukraine", "ua", "ukr"));
    descriptions->push_back(InputMethodDescriptor(
        "xkb:gb:extd:eng", "United Kingdom - Extended - Winkeys", "gb(extd)",
        "eng"));
    return descriptions;
  }

  InputMethodDescriptor previous_input_method_;
  InputMethodDescriptor current_input_method_;
  ImePropertyList current_ime_properties_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodLibraryStubImpl);
};

// static
InputMethodLibrary* InputMethodLibrary::GetImpl(bool stub) {
  if (stub)
    return new InputMethodLibraryStubImpl();
  else
    return new InputMethodLibraryImpl();
}

}  // namespace chromeos

// Allows InvokeLater without adding refcounting. This class is a Singleton and
// won't be deleted until it's last InvokeLater is run.
DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::InputMethodLibraryImpl);
