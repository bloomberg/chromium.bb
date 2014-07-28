// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_manager_impl.h"

#include <algorithm>  // std::find

#include "ash/ime/input_method_menu_item.h"
#include "ash/ime/input_method_menu_manager.h"
#include "base/basictypes.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/input_method/candidate_window_controller.h"
#include "chrome/browser/chromeos/input_method/component_extension_ime_manager_impl.h"
#include "chrome/browser/chromeos/input_method/input_method_engine.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ime/component_extension_ime_manager.h"
#include "chromeos/ime/extension_ime_util.h"
#include "chromeos/ime/fake_ime_keyboard.h"
#include "chromeos/ime/ime_keyboard.h"
#include "chromeos/ime/input_method_delegate.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "ui/base/accelerators/accelerator.h"

namespace chromeos {
namespace input_method {

namespace {

bool Contains(const std::vector<std::string>& container,
              const std::string& value) {
  return std::find(container.begin(), container.end(), value) !=
      container.end();
}

}  // namespace

bool InputMethodManagerImpl::IsLoginKeyboard(
    const std::string& layout) const {
  return util_.IsLoginKeyboard(layout);
}

bool InputMethodManagerImpl::MigrateInputMethods(
    std::vector<std::string>* input_method_ids) {
  return util_.MigrateInputMethods(input_method_ids);
}

InputMethodManagerImpl::InputMethodManagerImpl(
    scoped_ptr<InputMethodDelegate> delegate)
    : delegate_(delegate.Pass()),
      state_(STATE_LOGIN_SCREEN),
      util_(delegate_.get()),
      component_extension_ime_manager_(new ComponentExtensionIMEManager()),
      weak_ptr_factory_(this) {
  if (base::SysInfo::IsRunningOnChromeOS())
    keyboard_.reset(ImeKeyboard::Create());
  else
    keyboard_.reset(new FakeImeKeyboard());
}

InputMethodManagerImpl::~InputMethodManagerImpl() {
  if (candidate_window_controller_.get())
    candidate_window_controller_->RemoveObserver(this);
}

void InputMethodManagerImpl::AddObserver(
    InputMethodManager::Observer* observer) {
  observers_.AddObserver(observer);
}

void InputMethodManagerImpl::AddCandidateWindowObserver(
    InputMethodManager::CandidateWindowObserver* observer) {
  candidate_window_observers_.AddObserver(observer);
}

void InputMethodManagerImpl::RemoveObserver(
    InputMethodManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void InputMethodManagerImpl::RemoveCandidateWindowObserver(
    InputMethodManager::CandidateWindowObserver* observer) {
  candidate_window_observers_.RemoveObserver(observer);
}

void InputMethodManagerImpl::SetState(State new_state) {
  const State old_state = state_;
  state_ = new_state;
  switch (state_) {
    case STATE_LOGIN_SCREEN:
      break;
    case STATE_BROWSER_SCREEN:
      if (old_state == STATE_LOCK_SCREEN)
        OnScreenUnlocked();
      break;
    case STATE_LOCK_SCREEN:
      OnScreenLocked();
      break;
    case STATE_TERMINATING: {
      if (candidate_window_controller_.get())
        candidate_window_controller_.reset();
      break;
    }
  }
}

scoped_ptr<InputMethodDescriptors>
InputMethodManagerImpl::GetSupportedInputMethods() const {
  return scoped_ptr<InputMethodDescriptors>(new InputMethodDescriptors).Pass();
}

scoped_ptr<InputMethodDescriptors>
InputMethodManagerImpl::GetActiveInputMethods() const {
  scoped_ptr<InputMethodDescriptors> result(new InputMethodDescriptors);
  // Build the active input method descriptors from the active input
  // methods cache |active_input_method_ids_|.
  for (size_t i = 0; i < active_input_method_ids_.size(); ++i) {
    const std::string& input_method_id = active_input_method_ids_[i];
    const InputMethodDescriptor* descriptor =
        util_.GetInputMethodDescriptorFromId(input_method_id);
    if (descriptor) {
      result->push_back(*descriptor);
    } else {
      std::map<std::string, InputMethodDescriptor>::const_iterator ix =
          extra_input_methods_.find(input_method_id);
      if (ix != extra_input_methods_.end())
        result->push_back(ix->second);
      else
        DVLOG(1) << "Descriptor is not found for: " << input_method_id;
    }
  }
  if (result->empty()) {
    // Initially |active_input_method_ids_| is empty. browser_tests might take
    // this path.
    result->push_back(
        InputMethodUtil::GetFallbackInputMethodDescriptor());
  }
  return result.Pass();
}

const std::vector<std::string>&
InputMethodManagerImpl::GetActiveInputMethodIds() const {
  return active_input_method_ids_;
}

size_t InputMethodManagerImpl::GetNumActiveInputMethods() const {
  return active_input_method_ids_.size();
}

const InputMethodDescriptor* InputMethodManagerImpl::GetInputMethodFromId(
    const std::string& input_method_id) const {
  const InputMethodDescriptor* ime = util_.GetInputMethodDescriptorFromId(
      input_method_id);
  if (!ime) {
    std::map<std::string, InputMethodDescriptor>::const_iterator ix =
        extra_input_methods_.find(input_method_id);
    if (ix != extra_input_methods_.end())
      ime = &ix->second;
  }
  return ime;
}

void InputMethodManagerImpl::EnableLoginLayouts(
    const std::string& language_code,
    const std::vector<std::string>& initial_layouts) {
  if (state_ == STATE_TERMINATING)
    return;

  // First, hardware keyboard layout should be shown.
  std::vector<std::string> candidates =
      util_.GetHardwareLoginInputMethodIds();

  // Seocnd, locale based input method should be shown.
  // Add input methods associated with the language.
  std::vector<std::string> layouts_from_locale;
  util_.GetInputMethodIdsFromLanguageCode(language_code,
                                          kKeyboardLayoutsOnly,
                                          &layouts_from_locale);
  candidates.insert(candidates.end(), layouts_from_locale.begin(),
                    layouts_from_locale.end());

  std::vector<std::string> layouts;
  // First, add the initial input method ID, if it's requested, to
  // layouts, so it appears first on the list of active input
  // methods at the input language status menu.
  for (size_t i = 0; i < initial_layouts.size(); ++i) {
    if (util_.IsValidInputMethodId(initial_layouts[i])) {
      if (IsLoginKeyboard(initial_layouts[i])) {
        layouts.push_back(initial_layouts[i]);
      } else {
        DVLOG(1)
            << "EnableLoginLayouts: ignoring non-login initial keyboard layout:"
            << initial_layouts[i];
      }
    } else if (!initial_layouts[i].empty()) {
      DVLOG(1) << "EnableLoginLayouts: ignoring non-keyboard or invalid ID: "
               << initial_layouts[i];
    }
  }

  // Add candidates to layouts, while skipping duplicates.
  for (size_t i = 0; i < candidates.size(); ++i) {
    const std::string& candidate = candidates[i];
    // Not efficient, but should be fine, as the two vectors are very
    // short (2-5 items).
    if (!Contains(layouts, candidate) && IsLoginKeyboard(candidate))
      layouts.push_back(candidate);
  }

  MigrateInputMethods(&layouts);
  active_input_method_ids_.swap(layouts);

  // Initialize candidate window controller and widgets such as
  // candidate window, infolist and mode indicator.  Note, mode
  // indicator is used by only keyboard layout input methods.
  if (active_input_method_ids_.size() > 1)
    MaybeInitializeCandidateWindowController();

  // you can pass empty |initial_layout|.
  ChangeInputMethod(initial_layouts.empty() ? "" :
      extension_ime_util::GetInputMethodIDByEngineID(initial_layouts[0]));
}

// Adds new input method to given list.
bool InputMethodManagerImpl::EnableInputMethodImpl(
    const std::string& input_method_id,
    std::vector<std::string>* new_active_input_method_ids) const {
  DCHECK(new_active_input_method_ids);
  if (!util_.IsValidInputMethodId(input_method_id)) {
    DVLOG(1) << "EnableInputMethod: Invalid ID: " << input_method_id;
    return false;
  }

  if (!Contains(*new_active_input_method_ids, input_method_id))
    new_active_input_method_ids->push_back(input_method_id);

  return true;
}

// Starts or stops the system input method framework as needed.
void InputMethodManagerImpl::ReconfigureIMFramework() {
  LoadNecessaryComponentExtensions();

  // Initialize candidate window controller and widgets such as
  // candidate window, infolist and mode indicator.  Note, mode
  // indicator is used by only keyboard layout input methods.
  MaybeInitializeCandidateWindowController();
}

bool InputMethodManagerImpl::EnableInputMethod(
    const std::string& input_method_id) {
  if (!EnableInputMethodImpl(input_method_id, &active_input_method_ids_))
    return false;

  ReconfigureIMFramework();
  return true;
}

bool InputMethodManagerImpl::ReplaceEnabledInputMethods(
    const std::vector<std::string>& new_active_input_method_ids) {
  if (state_ == STATE_TERMINATING)
    return false;

  // Filter unknown or obsolete IDs.
  std::vector<std::string> new_active_input_method_ids_filtered;

  for (size_t i = 0; i < new_active_input_method_ids.size(); ++i)
    EnableInputMethodImpl(new_active_input_method_ids[i],
                          &new_active_input_method_ids_filtered);

  if (new_active_input_method_ids_filtered.empty()) {
    DVLOG(1) << "ReplaceEnabledInputMethods: No valid input method ID";
    return false;
  }

  // Copy extension IDs to |new_active_input_method_ids_filtered|. We have to
  // keep relative order of the extension input method IDs.
  for (size_t i = 0; i < active_input_method_ids_.size(); ++i) {
    const std::string& input_method_id = active_input_method_ids_[i];
    if (extension_ime_util::IsExtensionIME(input_method_id))
      new_active_input_method_ids_filtered.push_back(input_method_id);
  }
  active_input_method_ids_.swap(new_active_input_method_ids_filtered);
  MigrateInputMethods(&active_input_method_ids_);

  ReconfigureIMFramework();

  // If |current_input_method| is no longer in |active_input_method_ids_|,
  // ChangeInputMethod() picks the first one in |active_input_method_ids_|.
  ChangeInputMethod(current_input_method_.id());
  return true;
}

void InputMethodManagerImpl::ChangeInputMethod(
    const std::string& input_method_id) {
  ChangeInputMethodInternal(input_method_id, false);
}

bool InputMethodManagerImpl::ChangeInputMethodInternal(
    const std::string& input_method_id,
    bool show_message) {
  if (state_ == STATE_TERMINATING)
    return false;

  std::string input_method_id_to_switch = input_method_id;

  // Sanity check.
  if (!InputMethodIsActivated(input_method_id)) {
    scoped_ptr<InputMethodDescriptors> input_methods(GetActiveInputMethods());
    DCHECK(!input_methods->empty());
    input_method_id_to_switch = input_methods->at(0).id();
    if (!input_method_id.empty()) {
      DVLOG(1) << "Can't change the current input method to "
               << input_method_id << " since the engine is not enabled. "
               << "Switch to " << input_method_id_to_switch << " instead.";
    }
  }

  // Hide candidate window and info list.
  if (candidate_window_controller_.get())
    candidate_window_controller_->Hide();

  // Disable the current engine handler.
  IMEEngineHandlerInterface* engine =
      IMEBridge::Get()->GetCurrentEngineHandler();
  if (engine)
    engine->Disable();

  // Configure the next engine handler.
  if (InputMethodUtil::IsKeyboardLayout(input_method_id_to_switch) &&
      !extension_ime_util::IsKeyboardLayoutExtension(
          input_method_id_to_switch)) {
    IMEBridge::Get()->SetCurrentEngineHandler(NULL);
  } else {
    IMEEngineHandlerInterface* next_engine =
        profile_engine_map_[GetProfile()][input_method_id_to_switch];
    if (next_engine) {
      IMEBridge::Get()->SetCurrentEngineHandler(next_engine);
      next_engine->Enable();
    }
  }

  // TODO(komatsu): Check if it is necessary to perform the above routine
  // when the current input method is equal to |input_method_id_to_swich|.
  if (current_input_method_.id() != input_method_id_to_switch) {
    // Clear property list.  Property list would be updated by
    // extension IMEs via InputMethodEngine::(Set|Update)MenuItems.
    // If the current input method is a keyboard layout, empty
    // properties are sufficient.
    const ash::ime::InputMethodMenuItemList empty_menu_item_list;
    ash::ime::InputMethodMenuManager* input_method_menu_manager =
        ash::ime::InputMethodMenuManager::GetInstance();
    input_method_menu_manager->SetCurrentInputMethodMenuItemList(
            empty_menu_item_list);

    const InputMethodDescriptor* descriptor = NULL;
    if (extension_ime_util::IsExtensionIME(input_method_id_to_switch)) {
      DCHECK(extra_input_methods_.find(input_method_id_to_switch) !=
             extra_input_methods_.end());
      descriptor = &(extra_input_methods_[input_method_id_to_switch]);
    } else {
      descriptor =
          util_.GetInputMethodDescriptorFromId(input_method_id_to_switch);
      if (!descriptor)
        LOG(ERROR) << "Unknown input method id: " << input_method_id_to_switch;
    }
    DCHECK(descriptor);

    previous_input_method_ = current_input_method_;
    current_input_method_ = *descriptor;
  }

  // Change the keyboard layout to a preferred layout for the input method.
  if (!keyboard_->SetCurrentKeyboardLayoutByName(
          current_input_method_.GetPreferredKeyboardLayout())) {
    LOG(ERROR) << "Failed to change keyboard layout to "
               << current_input_method_.GetPreferredKeyboardLayout();
  }

  // Update input method indicators (e.g. "US", "DV") in Chrome windows.
  FOR_EACH_OBSERVER(InputMethodManager::Observer,
                    observers_,
                    InputMethodChanged(this, show_message));
  return true;
}

void InputMethodManagerImpl::LoadNecessaryComponentExtensions() {
  // Load component extensions but also update |active_input_method_ids_| as
  // some component extension IMEs may have been removed from the Chrome OS
  // image. If specified component extension IME no longer exists, falling back
  // to an existing IME.
  std::vector<std::string> unfiltered_input_method_ids;
  unfiltered_input_method_ids.swap(active_input_method_ids_);
  for (size_t i = 0; i < unfiltered_input_method_ids.size(); ++i) {
    if (!extension_ime_util::IsComponentExtensionIME(
        unfiltered_input_method_ids[i])) {
      // Legacy IMEs or xkb layouts are alwayes active.
      active_input_method_ids_.push_back(unfiltered_input_method_ids[i]);
    } else if (component_extension_ime_manager_->IsWhitelisted(
        unfiltered_input_method_ids[i])) {
      component_extension_ime_manager_->LoadComponentExtensionIME(
          unfiltered_input_method_ids[i]);
      active_input_method_ids_.push_back(unfiltered_input_method_ids[i]);
    }
  }
}

void InputMethodManagerImpl::ActivateInputMethodMenuItem(
    const std::string& key) {
  DCHECK(!key.empty());

  if (ash::ime::InputMethodMenuManager::GetInstance()->
      HasInputMethodMenuItemForKey(key)) {
    IMEEngineHandlerInterface* engine =
        IMEBridge::Get()->GetCurrentEngineHandler();
    if (engine)
      engine->PropertyActivate(key);
    return;
  }

  DVLOG(1) << "ActivateInputMethodMenuItem: unknown key: " << key;
}

void InputMethodManagerImpl::AddInputMethodExtension(
    const std::string& id,
    InputMethodEngineInterface* engine) {
  if (state_ == STATE_TERMINATING)
    return;

  DCHECK(engine);

  profile_engine_map_[GetProfile()][id] = engine;

  if (id == current_input_method_.id()) {
    IMEBridge::Get()->SetCurrentEngineHandler(engine);
    engine->Enable();
  }

  if (extension_ime_util::IsComponentExtensionIME(id))
    return;

  CHECK(extension_ime_util::IsExtensionIME(id))
      << id << "is not a valid extension input method ID";

  const InputMethodDescriptor& descriptor = engine->GetDescriptor();
  extra_input_methods_[id] = descriptor;

  if (Contains(enabled_extension_imes_, id)) {
    if (!Contains(active_input_method_ids_, id)) {
      active_input_method_ids_.push_back(id);
    } else {
      DVLOG(1) << "AddInputMethodExtension: alread added: "
               << id << ", " << descriptor.name();
    }

    // Ensure that the input method daemon is running.
    MaybeInitializeCandidateWindowController();
  }
}

void InputMethodManagerImpl::RemoveInputMethodExtension(const std::string& id) {
  if (!extension_ime_util::IsExtensionIME(id))
    DVLOG(1) << id << " is not a valid extension input method ID.";

  std::vector<std::string>::iterator i = std::find(
      active_input_method_ids_.begin(), active_input_method_ids_.end(), id);
  if (i != active_input_method_ids_.end())
    active_input_method_ids_.erase(i);
  extra_input_methods_.erase(id);

  EngineMap& engine_map = profile_engine_map_[GetProfile()];
  if (IMEBridge::Get()->GetCurrentEngineHandler() == engine_map[id])
    IMEBridge::Get()->SetCurrentEngineHandler(NULL);
  engine_map.erase(id);

  // No need to switch input method when terminating.
  if (state_ != STATE_TERMINATING) {
    // If |current_input_method| is no longer in |active_input_method_ids_|,
    // switch to the first one in |active_input_method_ids_|.
    ChangeInputMethod(current_input_method_.id());
  }
}

void InputMethodManagerImpl::GetInputMethodExtensions(
    InputMethodDescriptors* result) {
  // Build the extension input method descriptors from the extra input
  // methods cache |extra_input_methods_|.
  std::map<std::string, InputMethodDescriptor>::iterator iter;
  for (iter = extra_input_methods_.begin(); iter != extra_input_methods_.end();
       ++iter) {
    if (extension_ime_util::IsExtensionIME(iter->first))
      result->push_back(iter->second);
  }
}

void InputMethodManagerImpl::SetEnabledExtensionImes(
    std::vector<std::string>* ids) {
  enabled_extension_imes_.clear();
  enabled_extension_imes_.insert(enabled_extension_imes_.end(),
                                 ids->begin(),
                                 ids->end());

  bool active_imes_changed = false;

  for (std::map<std::string, InputMethodDescriptor>::iterator extra_iter =
       extra_input_methods_.begin(); extra_iter != extra_input_methods_.end();
       ++extra_iter) {
    if (extension_ime_util::IsComponentExtensionIME(
        extra_iter->first))
      continue;  // Do not filter component extension.
    std::vector<std::string>::iterator active_iter = std::find(
        active_input_method_ids_.begin(), active_input_method_ids_.end(),
        extra_iter->first);

    bool active = active_iter != active_input_method_ids_.end();
    bool enabled = Contains(enabled_extension_imes_, extra_iter->first);

    if (active && !enabled)
      active_input_method_ids_.erase(active_iter);

    if (!active && enabled)
      active_input_method_ids_.push_back(extra_iter->first);

    if (active == !enabled)
      active_imes_changed = true;
  }

  if (active_imes_changed) {
    MaybeInitializeCandidateWindowController();

    // If |current_input_method| is no longer in |active_input_method_ids_|,
    // switch to the first one in |active_input_method_ids_|.
    ChangeInputMethod(current_input_method_.id());
  }
}

void InputMethodManagerImpl::SetInputMethodLoginDefaultFromVPD(
    const std::string& locale, const std::string& oem_layout) {
  std::string layout;
  if (!oem_layout.empty()) {
    // If the OEM layout information is provided, use it.
    layout = oem_layout;
  } else {
    // Otherwise, determine the hardware keyboard from the locale.
    std::vector<std::string> input_method_ids;
    if (util_.GetInputMethodIdsFromLanguageCode(
        locale,
        chromeos::input_method::kKeyboardLayoutsOnly,
        &input_method_ids)) {
      // The output list |input_method_ids| is sorted by popularity, hence
      // input_method_ids[0] now contains the most popular keyboard layout
      // for the given locale.
      DCHECK_GE(input_method_ids.size(), 1U);
      layout = input_method_ids[0];
    }
  }

  if (layout.empty())
    return;

  std::vector<std::string> layouts;
  base::SplitString(layout, ',', &layouts);
  MigrateInputMethods(&layouts);

  PrefService* prefs = g_browser_process->local_state();
  prefs->SetString(prefs::kHardwareKeyboardLayout, JoinString(layouts, ","));

  // This asks the file thread to save the prefs (i.e. doesn't block).
  // The latest values of Local State reside in memory so we can safely
  // get the value of kHardwareKeyboardLayout even if the data is not
  // yet saved to disk.
  prefs->CommitPendingWrite();

  util_.UpdateHardwareLayoutCache();

  EnableLoginLayouts(locale, layouts);
  LoadNecessaryComponentExtensions();
}

void InputMethodManagerImpl::SetInputMethodLoginDefault() {
  // Set up keyboards. For example, when |locale| is "en-US", enable US qwerty
  // and US dvorak keyboard layouts.
  if (g_browser_process && g_browser_process->local_state()) {
    const std::string locale = g_browser_process->GetApplicationLocale();
    // If the preferred keyboard for the login screen has been saved, use it.
    PrefService* prefs = g_browser_process->local_state();
    std::string initial_input_method_id =
        prefs->GetString(chromeos::language_prefs::kPreferredKeyboardLayout);
    std::vector<std::string> input_methods_to_be_enabled;
    if (initial_input_method_id.empty()) {
      // If kPreferredKeyboardLayout is not specified, use the hardware layout.
      input_methods_to_be_enabled = util_.GetHardwareLoginInputMethodIds();
    } else {
      input_methods_to_be_enabled.push_back(initial_input_method_id);
    }
    EnableLoginLayouts(locale, input_methods_to_be_enabled);
    LoadNecessaryComponentExtensions();
  }
}

bool InputMethodManagerImpl::SwitchToNextInputMethod() {
  // Sanity checks.
  if (active_input_method_ids_.empty()) {
    DVLOG(1) << "active input method is empty";
    return false;
  }

  if (current_input_method_.id().empty()) {
    DVLOG(1) << "current_input_method_ is unknown";
    return false;
  }

  // Do not consume key event if there is only one input method is enabled.
  // Ctrl+Space or Alt+Shift may be used by other application.
  if (active_input_method_ids_.size() == 1)
    return false;

  // Find the next input method and switch to it.
  SwitchToNextInputMethodInternal(active_input_method_ids_,
                                  current_input_method_.id());
  return true;
}

bool InputMethodManagerImpl::SwitchToPreviousInputMethod(
    const ui::Accelerator& accelerator) {
  // Sanity check.
  if (active_input_method_ids_.empty()) {
    DVLOG(1) << "active input method is empty";
    return false;
  }

  // Do not consume key event if there is only one input method is enabled.
  // Ctrl+Space or Alt+Shift may be used by other application.
  if (active_input_method_ids_.size() == 1)
    return false;

  if (accelerator.type() == ui::ET_KEY_RELEASED)
    return true;

  if (previous_input_method_.id().empty() ||
      previous_input_method_.id() == current_input_method_.id()) {
    return SwitchToNextInputMethod();
  }

  std::vector<std::string>::const_iterator iter =
      std::find(active_input_method_ids_.begin(),
                active_input_method_ids_.end(),
                previous_input_method_.id());
  if (iter == active_input_method_ids_.end()) {
    // previous_input_method_ is not supported.
    return SwitchToNextInputMethod();
  }
  ChangeInputMethodInternal(*iter, true);
  return true;
}

bool InputMethodManagerImpl::SwitchInputMethod(
    const ui::Accelerator& accelerator) {
  // Sanity check.
  if (active_input_method_ids_.empty()) {
    DVLOG(1) << "active input method is empty";
    return false;
  }

  // Get the list of input method ids for the |accelerator|. For example, get
  // { "mozc-hangul", "xkb:kr:kr104:kor" } for ui::VKEY_DBE_SBCSCHAR.
  std::vector<std::string> input_method_ids_to_switch;
  switch (accelerator.key_code()) {
    case ui::VKEY_CONVERT:  // Henkan key on JP106 keyboard
      input_method_ids_to_switch.push_back(
          extension_ime_util::GetInputMethodIDByEngineID("nacl_mozc_jp"));
      break;
    case ui::VKEY_NONCONVERT:  // Muhenkan key on JP106 keyboard
      input_method_ids_to_switch.push_back(
          extension_ime_util::GetInputMethodIDByEngineID("xkb:jp::jpn"));
      break;
    case ui::VKEY_DBE_SBCSCHAR:  // ZenkakuHankaku key on JP106 keyboard
    case ui::VKEY_DBE_DBCSCHAR:
      input_method_ids_to_switch.push_back(
          extension_ime_util::GetInputMethodIDByEngineID("nacl_mozc_jp"));
      input_method_ids_to_switch.push_back(
          extension_ime_util::GetInputMethodIDByEngineID("xkb:jp::jpn"));
      break;
    default:
      NOTREACHED();
      break;
  }
  if (input_method_ids_to_switch.empty()) {
    DVLOG(1) << "Unexpected VKEY: " << accelerator.key_code();
    return false;
  }

  // Obtain the intersection of input_method_ids_to_switch and
  // active_input_method_ids_. The order of IDs in active_input_method_ids_ is
  // preserved.
  std::vector<std::string> ids;
  for (size_t i = 0; i < input_method_ids_to_switch.size(); ++i) {
    const std::string& id = input_method_ids_to_switch[i];
    if (Contains(active_input_method_ids_, id))
      ids.push_back(id);
  }
  if (ids.empty()) {
    // No input method for the accelerator is active. For example, we should
    // just ignore VKEY_HANGUL when mozc-hangul is not active.
    return false;
  }

  SwitchToNextInputMethodInternal(ids, current_input_method_.id());
  return true;  // consume the accelerator.
}

void InputMethodManagerImpl::SwitchToNextInputMethodInternal(
    const std::vector<std::string>& input_method_ids,
    const std::string& current_input_method_id) {
  std::vector<std::string>::const_iterator iter =
      std::find(input_method_ids.begin(),
                input_method_ids.end(),
                current_input_method_id);
  if (iter != input_method_ids.end())
    ++iter;
  if (iter == input_method_ids.end())
    iter = input_method_ids.begin();
  ChangeInputMethodInternal(*iter, true);
}

InputMethodDescriptor InputMethodManagerImpl::GetCurrentInputMethod() const {
  if (current_input_method_.id().empty())
    return InputMethodUtil::GetFallbackInputMethodDescriptor();

  return current_input_method_;
}

bool InputMethodManagerImpl::IsISOLevel5ShiftUsedByCurrentInputMethod() const {
  return keyboard_->IsISOLevel5ShiftAvailable();
}

bool InputMethodManagerImpl::IsAltGrUsedByCurrentInputMethod() const {
  return keyboard_->IsAltGrAvailable();
}

ImeKeyboard* InputMethodManagerImpl::GetImeKeyboard() {
  return keyboard_.get();
}

InputMethodUtil* InputMethodManagerImpl::GetInputMethodUtil() {
  return &util_;
}

ComponentExtensionIMEManager*
    InputMethodManagerImpl::GetComponentExtensionIMEManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return component_extension_ime_manager_.get();
}

void InputMethodManagerImpl::InitializeComponentExtension() {
  scoped_ptr<ComponentExtensionIMEManagerDelegate> delegate(
      new ComponentExtensionIMEManagerImpl());
  component_extension_ime_manager_->Initialize(delegate.Pass());

  util_.ResetInputMethods(
      component_extension_ime_manager_->GetAllIMEAsInputMethodDescriptor());
}

void InputMethodManagerImpl::SetCandidateWindowControllerForTesting(
    CandidateWindowController* candidate_window_controller) {
  candidate_window_controller_.reset(candidate_window_controller);
  candidate_window_controller_->AddObserver(this);
}

void InputMethodManagerImpl::SetImeKeyboardForTesting(ImeKeyboard* keyboard) {
  keyboard_.reset(keyboard);
}

void InputMethodManagerImpl::InitializeComponentExtensionForTesting(
    scoped_ptr<ComponentExtensionIMEManagerDelegate> delegate) {
  component_extension_ime_manager_->Initialize(delegate.Pass());
  util_.ResetInputMethods(
      component_extension_ime_manager_->GetAllIMEAsInputMethodDescriptor());
}

void InputMethodManagerImpl::CandidateClicked(int index) {
  IMEEngineHandlerInterface* engine =
      IMEBridge::Get()->GetCurrentEngineHandler();
  if (engine)
    engine->CandidateClicked(index);
}

void InputMethodManagerImpl::CandidateWindowOpened() {
  FOR_EACH_OBSERVER(InputMethodManager::CandidateWindowObserver,
                    candidate_window_observers_,
                    CandidateWindowOpened(this));
}

void InputMethodManagerImpl::CandidateWindowClosed() {
  FOR_EACH_OBSERVER(InputMethodManager::CandidateWindowObserver,
                    candidate_window_observers_,
                    CandidateWindowClosed(this));
}

void InputMethodManagerImpl::OnScreenLocked() {
  saved_previous_input_method_ = previous_input_method_;
  saved_current_input_method_ = current_input_method_;
  saved_active_input_method_ids_ = active_input_method_ids_;

  std::set<std::string> added_ids_;

  const std::vector<std::string>& hardware_keyboard_ids =
      util_.GetHardwareLoginInputMethodIds();

  active_input_method_ids_.clear();
  for (size_t i = 0; i < saved_active_input_method_ids_.size(); ++i) {
    const std::string& input_method_id = saved_active_input_method_ids_[i];
    // Skip if it's not a keyboard layout. Drop input methods including
    // extension ones.
    if (!IsLoginKeyboard(input_method_id) ||
        added_ids_.find(input_method_id) != added_ids_.end())
      continue;
    active_input_method_ids_.push_back(input_method_id);
    added_ids_.insert(input_method_id);
  }

  // We'll add the hardware keyboard if it's not included in
  // |active_input_method_ids_| so that the user can always use the hardware
  // keyboard on the screen locker.
  for (size_t i = 0; i < hardware_keyboard_ids.size(); ++i) {
    if (added_ids_.find(hardware_keyboard_ids[i]) == added_ids_.end()) {
      active_input_method_ids_.push_back(hardware_keyboard_ids[i]);
      added_ids_.insert(hardware_keyboard_ids[i]);
    }
  }

  ChangeInputMethod(current_input_method_.id());
}

void InputMethodManagerImpl::OnScreenUnlocked() {
  previous_input_method_ = saved_previous_input_method_;
  current_input_method_ = saved_current_input_method_;
  active_input_method_ids_ = saved_active_input_method_ids_;

  ChangeInputMethod(current_input_method_.id());
}

bool InputMethodManagerImpl::InputMethodIsActivated(
    const std::string& input_method_id) {
  return Contains(active_input_method_ids_, input_method_id);
}

void InputMethodManagerImpl::MaybeInitializeCandidateWindowController() {
  if (candidate_window_controller_.get())
    return;

  candidate_window_controller_.reset(
      CandidateWindowController::CreateCandidateWindowController());
  candidate_window_controller_->AddObserver(this);
}

Profile* InputMethodManagerImpl::GetProfile() const {
  return ProfileManager::GetActiveUserProfile();
}

}  // namespace input_method
}  // namespace chromeos
