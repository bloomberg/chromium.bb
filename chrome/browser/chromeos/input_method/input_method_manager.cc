// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_manager.h"

#include <algorithm>

#include <glib.h>

#include "unicode/uloc.h"

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/input_method/hotkey_manager.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/input_method/virtual_keyboard_selector.h"
#include "chrome/browser/chromeos/input_method/xkeyboard.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "googleurl/src/gurl.h"

#if !defined(USE_VIRTUAL_KEYBOARD)
#include "chrome/browser/chromeos/input_method/candidate_window.h"
#endif

#include <X11/X.h>  // ShiftMask, ControlMask, etc.
#include <X11/Xutil.h>  // for XK_* macros.

using content::BrowserThread;

namespace {

const char kIBusDaemonPath[] = "/usr/bin/ibus-daemon";
const size_t kMaxInputMethodsPerHotkey = 2;

// For hotkey handling.
enum HotkeyEvent {
  // Global hotkeys.
  kPreviousInputMethod = 0,
  kNextInputMethod,
  // Input method specific hotkeys.
  kJapaneseInputMethod,
  kJapaneseLayout,
  kJapaneseInputMethodOrLayout,
  kKoreanInputMethodOrLayout,
};

// Details of the input method specific hotkeys.
const struct InputMethodSpecificHotkeySetting {
  const char* input_method_ids[kMaxInputMethodsPerHotkey];
  int event_id;
  KeySym keysym;
  uint32 modifiers;
  bool trigger_on_press;  // if true. false means 'trigger on release'.
} kInputMethodSpecificHotkeySettings[] = {
  {
    { "mozc-jp" },
    kJapaneseInputMethod,
    XK_Henkan,
    0x0,
    true,
  },
  {
    { "xkb:jp::jpn" },
    kJapaneseLayout,
    XK_Muhenkan,
    0x0,
    true,
  },
  {
    { "mozc-jp", "xkb:jp::jpn" },
    kJapaneseInputMethodOrLayout,
    XK_Zenkaku_Hankaku,
    0x0,
    true,
  },
  {
    { "mozc-hangul", "xkb:kr:kr104:kor" },
    kKoreanInputMethodOrLayout,
    XK_Hangul,
    0x0,
    true,
  },
  {
    { "mozc-hangul", "xkb:kr:kr104:kor" },
    kKoreanInputMethodOrLayout,
    XK_space,
    ShiftMask,
    true,
  },
};

const size_t kInputMethodSpecificHotkeySettingsLen =
    arraysize(kInputMethodSpecificHotkeySettings);

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
namespace input_method {

// The implementation of InputMethodManager.
class InputMethodManagerImpl : public HotkeyManager::Observer,
                               public InputMethodManager,
                               public content::NotificationObserver,
                               public IBusController::Observer {
 public:
  InputMethodManagerImpl()
      : ibus_controller_(IBusController::Create()),
        should_launch_ime_(false),
        ime_connected_(false),
        defer_ime_startup_(false),
        enable_auto_ime_shutdown_(true),
        shutting_down_(false),
        ibus_daemon_process_handle_(base::kNullProcessHandle),
        util_(ibus_controller_->GetSupportedInputMethods()),
        xkeyboard_(util_) {
    // Observe APP_TERMINATING to stop input method daemon gracefully.
    // We should not use APP_EXITING here since logout might be canceled by
    // JavaScript after APP_EXITING is sent (crosbug.com/11055).
    // Note that even if we fail to stop input method daemon from
    // Chrome in case of a sudden crash, we have a way to do it from an
    // upstart script. See crosbug.com/6515 and crosbug.com/6995 for
    // details.
    notification_registrar_.Add(this, content::NOTIFICATION_APP_TERMINATING,
                                content::NotificationService::AllSources());

    // The observer should be added before Connect() so we can capture the
    // initial connection change.
    ibus_controller_->AddObserver(this);
    ibus_controller_->Connect();

    // Initialize extra_hotkeys_.
    for (size_t i = 0; i < kInputMethodSpecificHotkeySettingsLen; ++i) {
      const char* const* input_method_ids =
          kInputMethodSpecificHotkeySettings[i].input_method_ids;
      for (size_t j = 0;
           j < kMaxInputMethodsPerHotkey && input_method_ids[j];
           ++j) {
        extra_hotkeys_.insert(std::make_pair(
            input_method_ids[j], &kInputMethodSpecificHotkeySettings[i]));
      }
    }

    AddHotkeys();
    hotkey_manager_.AddObserver(this);
  }

  virtual ~InputMethodManagerImpl() {
    hotkey_manager_.RemoveObserver(this);
    ibus_controller_->RemoveObserver(this);
  }

  virtual void AddObserver(InputMethodManager::Observer* observer) {
    observers_.AddObserver(observer);
  }

  virtual void RemoveObserver(InputMethodManager::Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  virtual void AddPreLoginPreferenceObserver(
      InputMethodManager::PreferenceObserver* observer) {
    if (!pre_login_preference_observers_.size()) {
      observer->FirstObserverIsAdded(this);
    }
    pre_login_preference_observers_.AddObserver(observer);
  }

  virtual void RemovePreLoginPreferenceObserver(
      InputMethodManager::PreferenceObserver* observer) {
    pre_login_preference_observers_.RemoveObserver(observer);
  }

  virtual void AddPostLoginPreferenceObserver(
      InputMethodManager::PreferenceObserver* observer) {
    if (!post_login_preference_observers_.size()) {
      observer->FirstObserverIsAdded(this);
    }
    post_login_preference_observers_.AddObserver(observer);
  }

  virtual void RemovePostLoginPreferenceObserver(
      InputMethodManager::PreferenceObserver* observer) {
    post_login_preference_observers_.RemoveObserver(observer);
  }

  virtual void AddVirtualKeyboardObserver(VirtualKeyboardObserver* observer) {
    virtual_keyboard_observers_.AddObserver(observer);
  }

  virtual void RemoveVirtualKeyboardObserver(
      VirtualKeyboardObserver* observer) {
    virtual_keyboard_observers_.RemoveObserver(observer);
  }

  virtual InputMethodDescriptors* GetActiveInputMethods() {
    InputMethodDescriptors* result = new InputMethodDescriptors;
    // Build the active input method descriptors from the active input
    // methods cache |active_input_method_ids_|.
    for (size_t i = 0; i < active_input_method_ids_.size(); ++i) {
      const std::string& input_method_id = active_input_method_ids_[i];
      const InputMethodDescriptor* descriptor =
          util_.GetInputMethodDescriptorFromId(input_method_id);
      if (descriptor) {
        result->push_back(*descriptor);
      } else {
        std::map<std::string, InputMethodDescriptor>::
            const_iterator ix = extra_input_method_ids_.find(input_method_id);
        if (ix != extra_input_method_ids_.end()) {
          result->push_back(ix->second);
        } else {
          LOG(ERROR) << "Descriptor is not found for: " << input_method_id;
        }
      }
    }
    // Initially active_input_method_ids_ is empty. In this case, just
    // returns the fallback input method descriptor.
    if (result->empty()) {
      // Since browser_tests call neither SetImeConfig("preload_engines") nor
      // EnableInputMethod(), this path might be taken.
      result->push_back(
          InputMethodDescriptor::GetFallbackInputMethodDescriptor());
    }
    return result;
  }

  virtual size_t GetNumActiveInputMethods() {
    scoped_ptr<InputMethodDescriptors> input_methods(GetActiveInputMethods());
    return input_methods->size();
  }

  virtual InputMethodDescriptors* GetSupportedInputMethods() {
    return ibus_controller_->GetSupportedInputMethods();
  }

  virtual void ChangeInputMethod(const std::string& input_method_id) {
    // Changing the input method isn't guaranteed to succeed here, but we
    // should remember the last one regardless. See comments in
    // FlushImeConfig() for details.
    tentative_current_input_method_id_ = input_method_id;
    // If the input method daemon is not running and the specified input
    // method is a keyboard layout, switch the keyboard directly.
    if (ibus_daemon_process_handle_ == base::kNullProcessHandle &&
        InputMethodUtil::IsKeyboardLayout(input_method_id)) {
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

  virtual void EnableInputMethods(const std::string& language_code,
                                  InputMethodType type,
                                  const std::string& initial_input_method_id) {
    std::vector<std::string> candidates;
    // Add input methods associated with the language.
    util_.GetInputMethodIdsFromLanguageCode(language_code, type, &candidates);
    // Add the hardware keyboard as well. We should always add this so users
    // can use the hardware keyboard on the login screen and the screen locker.
    candidates.push_back(util_.GetHardwareInputMethodId());

    std::vector<std::string> input_method_ids;
    // First, add the initial input method ID, if it's requested, to
    // input_method_ids, so it appears first on the list of active input
    // methods at the input language status menu.
    if (!initial_input_method_id.empty()) {
      input_method_ids.push_back(initial_input_method_id);
    }

    // Add candidates to input_method_ids, while skipping duplicates.
    for (size_t i = 0; i < candidates.size(); ++i) {
      const std::string& candidate = candidates[i];
      // Not efficient, but should be fine, as the two vectors are very
      // short (2-5 items).
      if (std::count(input_method_ids.begin(), input_method_ids.end(),
                     candidate) == 0) {
        input_method_ids.push_back(candidate);
      }
    }

    // Update ibus-daemon setting. Here, we don't save the input method list
    // in the user's preferences.
    ImeConfigValue value;
    value.type = ImeConfigValue::kValueTypeStringList;
    value.string_list_value = input_method_ids;
    SetImeConfig(language_prefs::kGeneralSectionName,
                 language_prefs::kPreloadEnginesConfigName,
                 value);

    // Finaly, change to the initial input method, as needed.
    if (!initial_input_method_id.empty()) {
      ChangeInputMethod(initial_input_method_id);
    }
  }

  virtual void SetImePropertyActivated(const std::string& key,
                                       bool activated) {
    DCHECK(!key.empty());
    ibus_controller_->SetImePropertyActivated(key, activated);
  }

  virtual bool InputMethodIsActivated(const std::string& input_method_id) {
    scoped_ptr<InputMethodDescriptors> active_input_method_descriptors(
        GetActiveInputMethods());
    for (size_t i = 0; i < active_input_method_descriptors->size(); ++i) {
      if (active_input_method_descriptors->at(i).id() == input_method_id) {
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

      std::map<std::string, InputMethodDescriptor>::const_iterator ix;
      for (ix = extra_input_method_ids_.begin();
           ix != extra_input_method_ids_.end(); ++ix) {
        active_input_method_ids_.push_back(ix->first);
      }

      UpdateInputMethodSpecificHotkeys();
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

  // TODO(yusukes): Support input method specific hotkeys.
  virtual void AddActiveIme(const std::string& id,
                            const std::string& name,
                            const std::vector<std::string>& layouts,
                            const std::string& language) {
    std::string virtual_layouts = JoinString(layouts, ',');

    extra_input_method_ids_[id] = ibus_controller_->CreateInputMethodDescriptor(
        id, name, virtual_layouts, language);
    active_input_method_ids_.push_back(id);
    // TODO(yusukes): Call UpdateInputMethodSpecificHotkeys() here once IME
    // extension supports hotkeys.
  }

  virtual void RemoveActiveIme(const std::string& id) {
    std::vector<std::string>::iterator ix =
        std::find(active_input_method_ids_.begin(),
                  active_input_method_ids_.end(),
                  id);
    if (ix != active_input_method_ids_.end()) {
      active_input_method_ids_.erase(ix);
    }
    extra_input_method_ids_.erase(id);
    // TODO(yusukes): Call UpdateInputMethodSpecificHotkeys() here once IME
    // extension supports hotkeys.
  }

  virtual bool GetExtraDescriptor(const std::string& id,
                                  InputMethodDescriptor* descriptor) {
    std::map<std::string, InputMethodDescriptor>::const_iterator ix =
        extra_input_method_ids_.find(id);
    if (ix != extra_input_method_ids_.end()) {
      *descriptor = ix->second;
      return true;
    } else {
      return false;
    }
  }

  virtual InputMethodDescriptor previous_input_method() const {
    if (previous_input_method_.id().empty()) {
      return InputMethodDescriptor::GetFallbackInputMethodDescriptor();
    }
    return previous_input_method_;
  }

  virtual InputMethodDescriptor current_input_method() const {
    if (current_input_method_.id().empty()) {
      return InputMethodDescriptor::GetFallbackInputMethodDescriptor();
    }
    return current_input_method_;
  }

  virtual const ImePropertyList& current_ime_properties() const {
    return current_ime_properties_;
  }

  virtual void SendHandwritingStroke(const HandwritingStroke& stroke) {
    ibus_controller_->SendHandwritingStroke(stroke);
  }

  virtual void CancelHandwritingStrokes(int stroke_count) {
    // TODO(yusukes): Rename the function to CancelHandwritingStrokes.
    ibus_controller_->CancelHandwriting(stroke_count);
  }

  virtual void RegisterVirtualKeyboard(const GURL& launch_url,
                                       const std::string& name,
                                       const std::set<std::string>& layouts,
                                       bool is_system) {
    virtual_keyboard_selector_.AddVirtualKeyboard(launch_url,
                                                  name,
                                                  layouts,
                                                  is_system);
  }

  virtual const std::map<GURL, const VirtualKeyboard*>&
  GetUrlToKeyboardMapping() const {
    return virtual_keyboard_selector_.url_to_keyboard();
  }

  virtual const std::multimap<std::string, const VirtualKeyboard*>&
  GetLayoutNameToKeyboardMapping() const {
    return virtual_keyboard_selector_.layout_to_keyboard();
  }

  virtual bool SetVirtualKeyboardPreference(const std::string& input_method_id,
                                            const GURL& extention_url) {
    const bool result = virtual_keyboard_selector_.SetUserPreference(
        input_method_id, extention_url);
    UpdateVirtualKeyboardUI();
    return result;
  }

  virtual void ClearAllVirtualKeyboardPreferences() {
    virtual_keyboard_selector_.ClearAllUserPreferences();
    UpdateVirtualKeyboardUI();
  }

  virtual InputMethodUtil* GetInputMethodUtil() {
    return &util_;
  }

  virtual XKeyboard* GetXKeyboard() {
    return &xkeyboard_;
  }

  virtual HotkeyManager* GetHotkeyManager() {
    return &hotkey_manager_;
  }

  virtual void HotkeyPressed(HotkeyManager* manager, int event_id) {
    const HotkeyEvent event = HotkeyEvent(event_id);
    switch (event) {
      case kPreviousInputMethod:
        SwitchToPreviousInputMethod();
        break;
      case kNextInputMethod:
        SwitchToNextInputMethod();
        break;
      case kJapaneseInputMethod:
      case kJapaneseLayout:
      case kJapaneseInputMethodOrLayout:
      case kKoreanInputMethodOrLayout:
        SwitchToInputMethod(event);
        break;
    }
  }

  virtual void SwitchToNextInputMethod() {
    // Sanity checks.
    if (active_input_method_ids_.empty()) {
      LOG(ERROR) << "active input method is empty";
      return;
    }
    if (current_input_method_.id().empty()) {
      LOG(ERROR) << "current_input_method_ is unknown";
      return;
    }

    // Find the next input method.
    std::vector<std::string>::const_iterator iter =
        std::find(active_input_method_ids_.begin(),
                  active_input_method_ids_.end(),
                  current_input_method_.id());
    if (iter != active_input_method_ids_.end()) {
      ++iter;
    }
    if (iter == active_input_method_ids_.end()) {
      iter = active_input_method_ids_.begin();
    }
    ChangeInputMethod(*iter);
  }

  virtual void AddHotkeys() {
    // Add Ctrl+space.
    hotkey_manager_.AddHotkey(kPreviousInputMethod,
                              XK_space,
                              ControlMask,
                              true /* trigger on key press */);

    // Add Alt+Shift. For this hotkey, we use "trigger on key release" so that
    // the current IME is not changed on pressing Alt+Shift+something.
    hotkey_manager_.AddHotkey(kNextInputMethod,
                              XK_Shift_L,
                              ShiftMask | Mod1Mask,
                              false /* trigger on key release. */);
    hotkey_manager_.AddHotkey(kNextInputMethod,
                              XK_Shift_R,
                              ShiftMask | Mod1Mask,
                              false);
    hotkey_manager_.AddHotkey(kNextInputMethod,
                              XK_Meta_L,
                              ShiftMask | Mod1Mask,
                              false);
    hotkey_manager_.AddHotkey(kNextInputMethod,
                              XK_Meta_R,
                              ShiftMask | Mod1Mask,
                              false);
    // Input method specific hotkeys will be added in SetImeConfig().
  }

  virtual void RemoveHotkeys() {
    hotkey_manager_.RemoveHotkey(kPreviousInputMethod);
    hotkey_manager_.RemoveHotkey(kNextInputMethod);
  }

  static InputMethodManagerImpl* GetInstance() {
    return Singleton<InputMethodManagerImpl,
        DefaultSingletonTraits<InputMethodManagerImpl> >::get();
  }

 private:
  friend struct DefaultSingletonTraits<InputMethodManagerImpl>;

  // Returns true if the given input method config value is a string list
  // that only contains an input method ID of a keyboard layout.
  bool ContainOnlyKeyboardLayout(const ImeConfigValue& value) {
    if (value.type != ImeConfigValue::kValueTypeStringList) {
      return false;
    }
    for (size_t i = 0; i < value.string_list_value.size(); ++i) {
      if (!InputMethodUtil::IsKeyboardLayout(value.string_list_value[i])) {
        return false;
      }
    }
    return true;
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
        config_name == language_prefs::kPreloadEnginesConfigName &&
        value.type == ImeConfigValue::kValueTypeStringList &&
        !value.string_list_value.empty()) {
      // If there is only one input method which is a keyboard layout,
      // we don't start the input method processes.  When
      // |defer_ime_startup_| is true, we don't start it either.
      if (ContainOnlyKeyboardLayout(value) || defer_ime_startup_) {
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
        tentative_current_input_method_id_ = current_input_method_.id();
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
                                  const ImeConfigValue& value) {
    // If there is only one input method which is a keyboard layout,
    // and |enable_auto_ime_shutdown_| is true, we'll stop the input
    // method daemon.
    if (section == language_prefs::kGeneralSectionName &&
        config_name == language_prefs::kPreloadEnginesConfigName &&
        ContainOnlyKeyboardLayout(value) &&
        enable_auto_ime_shutdown_) {
      if (StopInputMethodDaemon()) {
        // The process is killed. Change the current keyboard layout.
        // We shouldn't use SetCurrentKeyboardLayoutByName() here. See
        // comments at ChangeCurrentInputMethod() for details.
        ChangeCurrentInputMethodFromId(value.string_list_value[0]);
      }
    }
  }

  // Change the keyboard layout per input method configuration being
  // updated, if necessary. See also: MaybeStartInputMethodDaemon().
  void MaybeChangeCurrentKeyboardLayout(const std::string& section,
                                        const std::string& config_name,
                                        const ImeConfigValue& value) {
    if (section == language_prefs::kGeneralSectionName &&
        config_name == language_prefs::kPreloadEnginesConfigName) {
      if (value.string_list_value.size() == 1 ||
          (value.string_list_value.size() != 0 &&
           current_input_method_.id().empty())) {
        // This is necessary to initialize current_input_method_. This is also
        // necessary when the current layout (e.g. INTL) out of two (e.g. US and
        // INTL) is disabled.
        ChangeCurrentInputMethodFromId(value.string_list_value[0]);
      }
      DCHECK(!current_input_method_.id().empty());

      // Update the indicator.
      // TODO(yusukes): Remove ActiveInputMethodsChanged notification in
      // FlushImeConfig().
      const size_t num_active_input_methods = GetNumActiveInputMethods();
      FOR_EACH_OBSERVER(InputMethodManager::Observer, observers_,
                        ActiveInputMethodsChanged(this,
                                                  current_input_method(),
                                                  num_active_input_methods));
    }
  }

  // Changes the current input method to |input_method_id| via IBus
  // daemon. If the id is not in the preload_engine list, this function
  // changes the current method to the first preloaded engine. Returns
  // true if the current engine is switched to |input_method_id| or the
  // first one.
  bool ChangeInputMethodViaIBus(const std::string& input_method_id) {
    std::string input_method_id_to_switch = input_method_id;

    if (!InputMethodIsActivated(input_method_id)) {
      // This path might be taken if prefs::kLanguageCurrentInputMethod (NOT
      // synced with cloud) and kLanguagePreloadEngines (synced with cloud) are
      // mismatched. e.g. the former is 'xkb:us::eng' and the latter (on the
      // sync server) is 'xkb:jp::jpn,mozc'.
      scoped_ptr<InputMethodDescriptors> input_methods(GetActiveInputMethods());
      DCHECK(!input_methods->empty());
      if (!input_methods->empty()) {
        input_method_id_to_switch = input_methods->at(0).id();
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
    bool active_input_methods_are_changed = false;
    InputMethodConfigRequests::iterator iter = pending_config_requests_.begin();
    while (iter != pending_config_requests_.end()) {
      const std::string& section = iter->first.first;
      const std::string& config_name = iter->first.second;
      ImeConfigValue& value = iter->second;

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
      FOR_EACH_OBSERVER(InputMethodManager::Observer, observers_,
                        ActiveInputMethodsChanged(this,
                                                  current_input_method_,
                                                  num_active_input_methods));
    }
  }

  // IBusController override.
  virtual void OnCurrentInputMethodChanged(
      const InputMethodDescriptor& current_input_method) {
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
      const ImePropertyList& prop_list) {
    // See comments in InputMethodChangedHandler.
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      LOG(ERROR) << "Not on UI thread";
      return;
    }

    RegisterProperties(prop_list);
  }

  // IBusController override.
  virtual void OnUpdateImeProperty(
      const ImePropertyList& prop_list) {
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
      pending_config_requests_.insert(current_config_values_.begin(),
                                      current_config_values_.end());
      FlushImeConfig();

      ChangeInputMethod(previous_input_method().id());
      ChangeInputMethod(current_input_method().id());
    }
  }

  // Ask the first observer to update preferences. We should not ask every
  // observer to do so. Otherwise, we'll end up updating preferences many times
  // when many observers are attached (ex. many windows are opened), which is
  // unnecessary and expensive.
  bool UpdateInputMethodPreference(
      ObserverList<PreferenceObserver>* observers) {
    ObserverListBase<PreferenceObserver>::Iterator it(*observers);
    PreferenceObserver* first_observer = it.GetNext();
    if (!first_observer) {
      return false;
    }
    first_observer->PreferenceUpdateNeeded(this,
                                           previous_input_method_,
                                           current_input_method_);
    return true;
  }

  // Changes the current input method from the given input method
  // descriptor.  This function updates states like current_input_method_
  // and notifies observers about the change (that will update the
  // preferences), hence this function should always be used even if you
  // just need to change the current keyboard layout.
  void ChangeCurrentInputMethod(const InputMethodDescriptor& new_input_method) {
    if (current_input_method_.id() != new_input_method.id()) {
      previous_input_method_ = current_input_method_;
      current_input_method_ = new_input_method;

      // Change the keyboard layout to a preferred layout for the input method.
      if (!xkeyboard_.SetCurrentKeyboardLayoutByName(
              current_input_method_.keyboard_layout())) {
        LOG(ERROR) << "Failed to change keyboard layout to "
                   << current_input_method_.keyboard_layout();
      }

      // Save the input method names to the Pref.
      if (!UpdateInputMethodPreference(&post_login_preference_observers_)) {
        // When both pre- and post-login observers are available, only notifies
        // the latter one.
        UpdateInputMethodPreference(&pre_login_preference_observers_);
      }
    }

    // Update input method indicators (e.g. "US", "DV") in Chrome windows.
    // For now, we have to do this every time to keep indicators updated. See
    // comments near the FOR_EACH_OBSERVER call in FlushImeConfig() for details.
    const size_t num_active_input_methods = GetNumActiveInputMethods();
    FOR_EACH_OBSERVER(InputMethodManager::Observer, observers_,
                      InputMethodChanged(this,
                                         current_input_method_,
                                         num_active_input_methods));

    UpdateVirtualKeyboardUI();
  }

  void UpdateVirtualKeyboardUI() {
#if defined(USE_VIRTUAL_KEYBOARD)
    const VirtualKeyboard* virtual_keyboard = NULL;
    std::string virtual_keyboard_layout = "";

    const std::vector<std::string>& layouts_vector =
        current_input_method_.virtual_keyboard_layouts();
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
                 << current_input_method_.id();

      // If the hardware is for US, show US Qwerty virtual keyboard.
      // If it's for France, show Azerty one.
      const std::string fallback_id = GetInputMethodUtil()->
          GetHardwareInputMethodId();
      const InputMethodDescriptor* fallback_desc =
          GetInputMethodUtil()->GetInputMethodDescriptorFromId(fallback_id);

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
                 << current_input_method_.id()
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
#endif  // USE_VIRTUAL_KEYBOARD
  }

  // Changes the current input method from the given input method ID.
  // This function is just a wrapper of ChangeCurrentInputMethod().
  void ChangeCurrentInputMethodFromId(const std::string& input_method_id) {
    const InputMethodDescriptor* descriptor =
        util_.GetInputMethodDescriptorFromId(input_method_id);
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

    // Update input method menu
    FOR_EACH_OBSERVER(InputMethodManager::Observer, observers_,
                      PropertyListChanged(this, current_ime_properties_));
  }

  // Starts the input method daemon. Unlike MaybeStopInputMethodDaemon(),
  // this function always starts the daemon. Returns true if the daemon is
  // started. Otherwise, e.g. the daemon is already started, returns false.
  bool StartInputMethodDaemon() {
    should_launch_ime_ = true;
    return MaybeLaunchInputMethodDaemon();
  }

  // Updates the properties used by the current input method.
  void UpdateProperty(const ImePropertyList& prop_list) {
    for (size_t i = 0; i < prop_list.size(); ++i) {
      FindAndUpdateProperty(prop_list[i], &current_ime_properties_);
    }

    // Update input method menu
    FOR_EACH_OBSERVER(InputMethodManager::Observer, observers_,
                      PropertyListChanged(this, current_ime_properties_));
  }

  // Launches an input method procsess specified by the given command
  // line. On success, returns true and stores the process handle in
  // |process_handle|. Otherwise, returns false, and the contents of
  // |process_handle| is untouched. OnImeShutdown will be called when the
  // process terminates.
  bool LaunchInputMethodProcess(const std::string& command_line,
                                base::ProcessHandle* process_handle) {
    std::vector<std::string> argv;
    base::ProcessHandle handle = base::kNullProcessHandle;

    // TODO(zork): export "LD_PRELOAD=/usr/lib/libcrash.so"
    base::SplitString(command_line, ' ', &argv);

    if (!base::LaunchProcess(argv, base::LaunchOptions(), &handle)) {
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
    if (!should_launch_ime_) {
      return false;
    }

    if (shutting_down_) {
      NOTREACHED() << "Trying to launch input method while shutting down";
      return false;
    }

#if !defined(USE_VIRTUAL_KEYBOARD)
    if (!candidate_window_controller_.get()) {
      candidate_window_controller_.reset(new CandidateWindowController);
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
        base::StringPrintf(
            "%s --panel=disable --cache=none --restart --replace",
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
                            InputMethodManagerImpl* library) {
    if (library->ibus_daemon_process_handle_ != base::kNullProcessHandle &&
        base::GetProcId(library->ibus_daemon_process_handle_) == pid) {
      library->ibus_daemon_process_handle_ = base::kNullProcessHandle;
    }

    // Restart input method daemon if needed.
    library->MaybeLaunchInputMethodDaemon();
  }

  // Stops the backend input method daemon. This function should also be
  // called from MaybeStopInputMethodDaemon(), except one case where we
  // stop the input method daemon at Chrome shutdown in Observe(). Returns true
  // if the daemon is stopped. Otherwise, e.g. the daemon is already stopped,
  // returns false.
  bool StopInputMethodDaemon() {
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
      return true;
    }
    return false;
  }

  void SetDeferImeStartup(bool defer) {
    VLOG(1) << "Setting DeferImeStartup to " << defer;
    defer_ime_startup_ = defer;
  }

  void SetEnableAutoImeShutdown(bool enable) {
    enable_auto_ime_shutdown_ = enable;
  }

  // content::NotificationObserver implementation:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) {
    // Stop the input method daemon on browser shutdown.
    if (type == content::NOTIFICATION_APP_TERMINATING) {
      shutting_down_ = true;
      notification_registrar_.RemoveAll();
      StopInputMethodDaemon();
#if !defined(USE_VIRTUAL_KEYBOARD)
      candidate_window_controller_.reset(NULL);
#endif
    }
  }

  void UpdateInputMethodSpecificHotkeys() {
    // Remove all input method specific hotkeys first.
    for (size_t i = 0; i < kInputMethodSpecificHotkeySettingsLen; ++i) {
      hotkey_manager_.RemoveHotkey(
          kInputMethodSpecificHotkeySettings[i].event_id);
    }

    for (size_t i = 0; i < active_input_method_ids_.size(); ++i) {
      typedef std::map<std::string,
          const InputMethodSpecificHotkeySetting*>::const_iterator Iter;
      std::pair<Iter, Iter> result = extra_hotkeys_.equal_range(
          active_input_method_ids_[i]);
      for (Iter iter = result.first; iter != result.second; ++iter) {
        hotkey_manager_.AddHotkey(iter->second->event_id,
                                  iter->second->keysym,
                                  iter->second->modifiers,
                                  iter->second->trigger_on_press);
      }
    }
    // TODO(yusukes): Check hotkeys for IME extensions.
  }

  // Handles "Control+space" hotkey.
  void SwitchToPreviousInputMethod() {
    // Sanity check.
    if (active_input_method_ids_.empty()) {
      LOG(ERROR) << "active input method is empty";
      return;
    }

    if (previous_input_method_.id().empty() ||
        previous_input_method_.id() == current_input_method_.id()) {
      SwitchToNextInputMethod();
      return;
    }

    std::vector<std::string>::const_iterator iter =
        std::find(active_input_method_ids_.begin(),
                  active_input_method_ids_.end(),
                  previous_input_method_.id());
    if (iter == active_input_method_ids_.end()) {
      // previous_input_method_ is not supported.
      SwitchToNextInputMethod();
    } else {
      ChangeInputMethod(*iter);
    }
  }

  // Handles input method specific hotkeys like XK_Henkan.
  void SwitchToInputMethod(HotkeyEvent event) {
    // Sanity check.
    if (active_input_method_ids_.empty()) {
      LOG(ERROR) << "active input method is empty";
      return;
    }

    // Get the list of input method ids for |event_id|. For example, get
    // { "mozc-hangul", "xkb:kr:kr104:kor" } for kKoreanInputMethodOrLayout.
    const char* const* input_method_ids_to_switch = NULL;
    switch (event) {
      case kPreviousInputMethod:
      case kNextInputMethod:
        // These events should not be handled here.
        break;
      case kJapaneseInputMethod:
      case kJapaneseLayout:
      case kJapaneseInputMethodOrLayout:
      case kKoreanInputMethodOrLayout:
        for (size_t i = 0; i < kInputMethodSpecificHotkeySettingsLen; ++i) {
          if (event == kInputMethodSpecificHotkeySettings[i].event_id) {
            input_method_ids_to_switch =
                kInputMethodSpecificHotkeySettings[i].input_method_ids;
          }
        }
        break;
    }
    if (!input_method_ids_to_switch) {
      LOG(ERROR) << "Unknown event: " << event;
      return;
    }

    // Obtain the intersection of input_method_ids_to_switch and
    // active_input_method_ids_. The order of IDs in active_input_method_ids_ is
    // preserved.
    std::vector<std::string> ids;
    for (size_t i = 0; i < kMaxInputMethodsPerHotkey; ++i) {
      const char* id = input_method_ids_to_switch[i];
      if (id && std::find(active_input_method_ids_.begin(),
                          active_input_method_ids_.end(),
                          id) != active_input_method_ids_.end()) {
        ids.push_back(id);
      }
    }
    if (ids.empty()) {
      LOG(ERROR) << "No input method for " << event << " is active";
      return;
    }

    // If current_input_method_ is not in ids, switch to ids[0]. If
    // current_input_method_ is ids[N], switch to ids[N+1].
    std::vector<std::string>::const_iterator iter =
        std::find(ids.begin(), ids.end(), current_input_method_.id());
    if (iter != ids.end()) {
      ++iter;
    }
    if (iter == ids.end()) {
      iter = ids.begin();
    }
    ChangeInputMethod(*iter);
  }

  // The IBus controller is used to control the input method status and
  // allow allow callbacks when the input method status changes.
  scoped_ptr<IBusController> ibus_controller_;
  ObserverList<InputMethodManager::Observer> observers_;
  ObserverList<PreferenceObserver> pre_login_preference_observers_;
  ObserverList<PreferenceObserver> post_login_preference_observers_;
  ObserverList<VirtualKeyboardObserver> virtual_keyboard_observers_;

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

  // This is used to register this object to APP_TERMINATING notification.
  content::NotificationRegistrar notification_registrar_;

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

  // The candidate window.  This will be deleted when the APP_TERMINATING
  // message is sent.
#if !defined(USE_VIRTUAL_KEYBOARD)
  scoped_ptr<CandidateWindowController> candidate_window_controller_;
#endif

  // True if we've received the APP_TERMINATING notification.
  bool shutting_down_;

  // The process handle of the IBus daemon. kNullProcessHandle if it's not
  // running.
  base::ProcessHandle ibus_daemon_process_handle_;

  // An object which keeps a list of available virtual keyboards.
  VirtualKeyboardSelector virtual_keyboard_selector_;

  // The active input method ids cache.
  std::vector<std::string> active_input_method_ids_;

  // Extra input methods that have been explicitly added to the menu, such as
  // those created by extension.
  std::map<std::string, InputMethodDescriptor> extra_input_method_ids_;

  // A muitlmap from input method id to input method specific hotkey
  // information. e.g. "mozc-jp" to XK_ZenkakuHankaku, "mozc-jp" to XK_Henkan.
  std::multimap<std::string,
                const InputMethodSpecificHotkeySetting*> extra_hotkeys_;

  // An object which provides miscellaneous input method utility functions. Note
  // that |util_| is required to initialize |xkeyboard_|.
  InputMethodUtil util_;

  // An object for switching XKB layouts and keyboard status like caps lock and
  // auto-repeat interval.
  XKeyboard xkeyboard_;

  // An object which detects Control+space and Shift+Alt key presses.
  HotkeyManager hotkey_manager_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodManagerImpl);
};

// static
InputMethodManager* InputMethodManager::GetInstance() {
  return InputMethodManagerImpl::GetInstance();
}

}  // namespace input_method
}  // namespace chromeos

// Allows InvokeLater without adding refcounting. This class is a Singleton and
// won't be deleted until it's last InvokeLater is run.
DISABLE_RUNNABLE_METHOD_REFCOUNT(
    chromeos::input_method::InputMethodManagerImpl);
