// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_manager_impl.h"

#include <algorithm>  // std::find

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "chrome/browser/chromeos/input_method/candidate_window_controller.h"
#include "chrome/browser/chromeos/input_method/input_method_delegate.h"
#include "chrome/browser/chromeos/input_method/input_method_engine_ibus.h"
#include "chrome/browser/chromeos/input_method/xkeyboard.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "ui/base/accelerators/accelerator.h"
#include "unicode/uloc.h"

namespace chromeos {
namespace input_method {

namespace {

bool Contains(const std::vector<std::string>& container,
              const std::string& value) {
  return std::find(container.begin(), container.end(), value) !=
      container.end();
}

}  // namespace

InputMethodManagerImpl::InputMethodManagerImpl(
    scoped_ptr<InputMethodDelegate> delegate)
    : delegate_(delegate.Pass()),
      state_(STATE_LOGIN_SCREEN),
      util_(delegate_.get(), GetSupportedInputMethods()) {
}

InputMethodManagerImpl::~InputMethodManagerImpl() {
  if (ibus_controller_.get())
    ibus_controller_->RemoveObserver(this);
  if (candidate_window_controller_.get()) {
    candidate_window_controller_->RemoveObserver(this);
    candidate_window_controller_->Shutdown(ibus_controller_.get());
  }
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
      ibus_controller_->Stop();
      if (candidate_window_controller_.get()) {
        candidate_window_controller_->Shutdown(ibus_controller_.get());
        candidate_window_controller_.reset();
      }
      break;
    }
  }
}

scoped_ptr<InputMethodDescriptors>
InputMethodManagerImpl::GetSupportedInputMethods() const {
  return whitelist_.GetSupportedInputMethods();
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
        InputMethodDescriptor::GetFallbackInputMethodDescriptor());
  }
  return result.Pass();
}

size_t InputMethodManagerImpl::GetNumActiveInputMethods() const {
  return active_input_method_ids_.size();
}

void InputMethodManagerImpl::EnableLayouts(const std::string& language_code,
                                           const std::string& initial_layout) {
  if (state_ == STATE_TERMINATING)
    return;

  std::vector<std::string> candidates;
  // Add input methods associated with the language.
  util_.GetInputMethodIdsFromLanguageCode(language_code,
                                          kKeyboardLayoutsOnly,
                                          &candidates);
  // Add the hardware keyboard as well. We should always add this so users
  // can use the hardware keyboard on the login screen and the screen locker.
  candidates.push_back(util_.GetHardwareInputMethodId());

  std::vector<std::string> layouts;
  // First, add the initial input method ID, if it's requested, to
  // layouts, so it appears first on the list of active input
  // methods at the input language status menu.
  if (util_.IsValidInputMethodId(initial_layout) &&
      InputMethodUtil::IsKeyboardLayout(initial_layout)) {
    layouts.push_back(initial_layout);
  } else if (!initial_layout.empty()) {
    DVLOG(1) << "EnableLayouts: ignoring non-keyboard or invalid ID: "
             << initial_layout;
  }

  // Add candidates to layouts, while skipping duplicates.
  for (size_t i = 0; i < candidates.size(); ++i) {
    const std::string& candidate = candidates[i];
    // Not efficient, but should be fine, as the two vectors are very
    // short (2-5 items).
    if (!Contains(layouts, candidate))
      layouts.push_back(candidate);
  }

  active_input_method_ids_.swap(layouts);
  ChangeInputMethod(initial_layout);  // you can pass empty |initial_layout|.
}

bool InputMethodManagerImpl::EnableInputMethods(
    const std::vector<std::string>& new_active_input_method_ids) {
  if (state_ == STATE_TERMINATING)
    return false;

  // Filter unknown or obsolete IDs.
  std::vector<std::string> new_active_input_method_ids_filtered;

  for (size_t i = 0; i < new_active_input_method_ids.size(); ++i) {
    const std::string& input_method_id = new_active_input_method_ids[i];
    if (util_.IsValidInputMethodId(input_method_id))
      new_active_input_method_ids_filtered.push_back(input_method_id);
    else
      DVLOG(1) << "EnableInputMethods: Invalid ID: " << input_method_id;
  }

  if (new_active_input_method_ids_filtered.empty()) {
    DVLOG(1) << "EnableInputMethods: No valid input method ID";
    return false;
  }

  // Copy extension IDs to |new_active_input_method_ids_filtered|. We have to
  // keep relative order of the extension input method IDs.
  for (size_t i = 0; i < active_input_method_ids_.size(); ++i) {
    const std::string& input_method_id = active_input_method_ids_[i];
    if (InputMethodUtil::IsExtensionInputMethod(input_method_id))
      new_active_input_method_ids_filtered.push_back(input_method_id);
  }
  active_input_method_ids_.swap(new_active_input_method_ids_filtered);

  if (ContainOnlyKeyboardLayout(active_input_method_ids_)) {
    // Do NOT call ibus_controller_->Stop(); here to work around a crash issue
    // at crosbug.com/27051.
    // TODO(yusukes): We can safely call Stop(); here once crosbug.com/26443
    // is implemented.
  } else {
    MaybeInitializeCandidateWindowController();
    ibus_controller_->Start();
  }

  // If |current_input_method| is no longer in |active_input_method_ids_|,
  // ChangeInputMethod() picks the first one in |active_input_method_ids_|.
  ChangeInputMethod(current_input_method_.id());
  return true;
}

bool InputMethodManagerImpl::SetInputMethodConfig(
    const std::string& section,
    const std::string& config_name,
    const InputMethodConfigValue& value) {
  DCHECK(section != language_prefs::kGeneralSectionName ||
         config_name != language_prefs::kPreloadEnginesConfigName);

  if (state_ == STATE_TERMINATING)
    return false;
  return ibus_controller_->SetInputMethodConfig(section, config_name, value);
}

void InputMethodManagerImpl::ChangeInputMethod(
    const std::string& input_method_id) {
  ChangeInputMethodInternal(input_method_id, false);
}

void InputMethodManagerImpl::ChangeInputMethodInternal(
    const std::string& input_method_id,
    bool show_message) {
  if (state_ == STATE_TERMINATING)
    return;

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

  if (InputMethodUtil::IsKeyboardLayout(input_method_id_to_switch)) {
    FOR_EACH_OBSERVER(InputMethodManager::Observer,
                      observers_,
                      InputMethodPropertyChanged(this));
    // Hack for fixing http://crosbug.com/p/12798
    // We should notify IME switching to ibus-daemon, otherwise
    // IBusPreeditFocusMode does not work. To achieve it, change engine to
    // itself if the next engine is XKB layout.
    const std::string current_input_method_id = current_input_method_.id();
    if (current_input_method_id.empty() ||
        InputMethodUtil::IsKeyboardLayout(current_input_method_id)) {
      ibus_controller_->Reset();
    } else {
      ibus_controller_->ChangeInputMethod(current_input_method_id);
    }
  } else {
    ibus_controller_->ChangeInputMethod(input_method_id_to_switch);
  }

  if (current_input_method_.id() != input_method_id_to_switch) {
    const InputMethodDescriptor* descriptor = NULL;
    if (!InputMethodUtil::IsExtensionInputMethod(input_method_id_to_switch)) {
      descriptor =
          util_.GetInputMethodDescriptorFromId(input_method_id_to_switch);
    } else {
      std::map<std::string, InputMethodDescriptor>::const_iterator i =
          extra_input_methods_.find(input_method_id_to_switch);
      DCHECK(i != extra_input_methods_.end());
      descriptor = &(i->second);
    }
    DCHECK(descriptor);

    previous_input_method_ = current_input_method_;
    current_input_method_ = *descriptor;
  }

  // Change the keyboard layout to a preferred layout for the input method.
  if (!xkeyboard_->SetCurrentKeyboardLayoutByName(
          current_input_method_.keyboard_layout())) {
    LOG(ERROR) << "Failed to change keyboard layout to "
               << current_input_method_.keyboard_layout();
  }

  // Update input method indicators (e.g. "US", "DV") in Chrome windows.
  FOR_EACH_OBSERVER(InputMethodManager::Observer,
                    observers_,
                    InputMethodChanged(this, show_message));
}

void InputMethodManagerImpl::ActivateInputMethodProperty(
    const std::string& key) {
  DCHECK(!key.empty());
  ibus_controller_->ActivateInputMethodProperty(key);
}

void InputMethodManagerImpl::AddInputMethodExtension(
    const std::string& id,
    const std::string& name,
    const std::vector<std::string>& layouts,
    const std::string& language,
    InputMethodEngine* engine) {
  if (state_ == STATE_TERMINATING)
    return;

  if (!InputMethodUtil::IsExtensionInputMethod(id)) {
    DVLOG(1) << id << " is not a valid extension input method ID.";
    return;
  }

  const std::string layout = layouts.empty() ? "" : layouts[0];
  extra_input_methods_[id] =
      InputMethodDescriptor(id, name, layout, language, true);
  if (!Contains(filtered_extension_imes_, id)) {
    if (!Contains(active_input_method_ids_, id)) {
      active_input_method_ids_.push_back(id);
    } else {
      DVLOG(1) << "AddInputMethodExtension: alread added: "
               << id << ", " << name;
      // Call Start() anyway, just in case.
    }

    // Ensure that the input method daemon is running.
    MaybeInitializeCandidateWindowController();
    ibus_controller_->Start();
  }

  extra_input_method_instances_[id] =
      static_cast<InputMethodEngineIBus*>(engine);
}

void InputMethodManagerImpl::RemoveInputMethodExtension(const std::string& id) {
  if (!InputMethodUtil::IsExtensionInputMethod(id))
    DVLOG(1) << id << " is not a valid extension input method ID.";

  std::vector<std::string>::iterator i = std::find(
      active_input_method_ids_.begin(), active_input_method_ids_.end(), id);
  if (i != active_input_method_ids_.end())
    active_input_method_ids_.erase(i);
  extra_input_methods_.erase(id);

  if (ContainOnlyKeyboardLayout(active_input_method_ids_)) {
    // Do NOT call ibus_controller_->Stop(); here to work around a crash issue
    // at crosbug.com/27051.
    // TODO(yusukes): We can safely call Stop(); here once crosbug.com/26443
    // is implemented.
  }

  // If |current_input_method| is no longer in |active_input_method_ids_|,
  // switch to the first one in |active_input_method_ids_|.
  ChangeInputMethod(current_input_method_.id());

  std::map<std::string, InputMethodEngineIBus*>::iterator ite =
      extra_input_method_instances_.find(id);
  if (ite == extra_input_method_instances_.end()) {
    DVLOG(1) << "The engine instance of " << id << " has already gone.";
  } else {
    // Do NOT release the actual instance here. This class does not take an
    // onwership of engine instance.
    extra_input_method_instances_.erase(ite);
  }
}

void InputMethodManagerImpl::GetInputMethodExtensions(
    InputMethodDescriptors* result) {
  // Build the extension input method descriptors from the extra input
  // methods cache |extra_input_methods_|.
  std::map<std::string, InputMethodDescriptor>::iterator iter;
  for (iter = extra_input_methods_.begin(); iter != extra_input_methods_.end();
       ++iter) {
    result->push_back(iter->second);
  }
}

void InputMethodManagerImpl::SetFilteredExtensionImes(
    std::vector<std::string>* ids) {
  filtered_extension_imes_.clear();
  filtered_extension_imes_.insert(filtered_extension_imes_.end(),
                                  ids->begin(),
                                  ids->end());

  bool active_imes_changed = false;

  for (std::map<std::string, InputMethodDescriptor>::iterator extra_iter =
       extra_input_methods_.begin(); extra_iter != extra_input_methods_.end();
       ++extra_iter) {
    std::vector<std::string>::iterator active_iter = std::find(
        active_input_method_ids_.begin(), active_input_method_ids_.end(),
        extra_iter->first);

    bool active = active_iter != active_input_method_ids_.end();
    bool filtered = Contains(filtered_extension_imes_, extra_iter->first);

    if (active && filtered)
      active_input_method_ids_.erase(active_iter);

    if (!active && !filtered)
      active_input_method_ids_.push_back(extra_iter->first);

    if (active == filtered)
      active_imes_changed = true;
  }

  if (active_imes_changed) {
    MaybeInitializeCandidateWindowController();
    ibus_controller_->Start();

    // If |current_input_method| is no longer in |active_input_method_ids_|,
    // switch to the first one in |active_input_method_ids_|.
    ChangeInputMethod(current_input_method_.id());
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

  // Find the next input method and switch to it.
  SwitchToNextInputMethodInternal(active_input_method_ids_,
                                  current_input_method_.id());
  return true;
}

bool InputMethodManagerImpl::SwitchToPreviousInputMethod() {
  // Sanity check.
  if (active_input_method_ids_.empty()) {
    DVLOG(1) << "active input method is empty";
    return false;
  }

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
      input_method_ids_to_switch.push_back("mozc-jp");
      break;
    case ui::VKEY_NONCONVERT:  // Muhenkan key on JP106 keyboard
      input_method_ids_to_switch.push_back("xkb:jp::jpn");
      break;
    case ui::VKEY_DBE_SBCSCHAR:  // ZenkakuHankaku key on JP106 keyboard
    case ui::VKEY_DBE_DBCSCHAR:
      input_method_ids_to_switch.push_back("mozc-jp");
      input_method_ids_to_switch.push_back("xkb:jp::jpn");
      break;
    case ui::VKEY_HANGUL:  // Hangul (or right Alt) key on Korean keyboard
      input_method_ids_to_switch.push_back("mozc-hangul");
      input_method_ids_to_switch.push_back("xkb:kr:kr104:kor");
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
    return InputMethodDescriptor::GetFallbackInputMethodDescriptor();
  return current_input_method_;
}

InputMethodPropertyList
InputMethodManagerImpl::GetCurrentInputMethodProperties() const {
  // This check is necessary since an IME property (e.g. for Pinyin) might be
  // sent from ibus-daemon AFTER the current input method is switched to XKB.
  if (InputMethodUtil::IsKeyboardLayout(GetCurrentInputMethod().id()))
    return InputMethodPropertyList();
  return ibus_controller_->GetCurrentProperties();
}

XKeyboard* InputMethodManagerImpl::GetXKeyboard() {
  return xkeyboard_.get();
}

InputMethodUtil* InputMethodManagerImpl::GetInputMethodUtil() {
  return &util_;
}

void InputMethodManagerImpl::OnConnected() {
  for (std::map<std::string, InputMethodEngineIBus*>::iterator ite =
          extra_input_method_instances_.begin();
       ite != extra_input_method_instances_.end();
       ite++) {
    if (!Contains(filtered_extension_imes_, ite->first))
      ite->second->OnConnected();
  }
}

void InputMethodManagerImpl::OnDisconnected() {
  for (std::map<std::string, InputMethodEngineIBus*>::iterator ite =
          extra_input_method_instances_.begin();
       ite != extra_input_method_instances_.end();
       ite++) {
    if (!Contains(filtered_extension_imes_, ite->first))
      ite->second->OnDisconnected();
  }
}

void InputMethodManagerImpl::Init() {
  DCHECK(!ibus_controller_.get());

  ibus_controller_.reset(IBusController::Create(
      delegate_->GetDefaultTaskRunner(), delegate_->GetWorkerTaskRunner()));
  xkeyboard_.reset(XKeyboard::Create(util_, delegate_->GetDefaultTaskRunner()));
  ibus_controller_->AddObserver(this);
}

void InputMethodManagerImpl::SetIBusControllerForTesting(
    IBusController* ibus_controller) {
  ibus_controller_.reset(ibus_controller);
  ibus_controller_->AddObserver(this);
}

void InputMethodManagerImpl::SetCandidateWindowControllerForTesting(
    CandidateWindowController* candidate_window_controller) {
  candidate_window_controller_.reset(candidate_window_controller);
  candidate_window_controller_->Init(ibus_controller_.get());
  candidate_window_controller_->AddObserver(this);
}

void InputMethodManagerImpl::SetXKeyboardForTesting(XKeyboard* xkeyboard) {
  xkeyboard_.reset(xkeyboard);
}

void InputMethodManagerImpl::PropertyChanged() {
  FOR_EACH_OBSERVER(InputMethodManager::Observer,
                    observers_,
                    InputMethodPropertyChanged(this));
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

  const std::string hardware_keyboard_id = util_.GetHardwareInputMethodId();
  // We'll add the hardware keyboard if it's not included in
  // |active_input_method_list| so that the user can always use the hardware
  // keyboard on the screen locker.
  bool should_add_hardware_keyboard = true;

  active_input_method_ids_.clear();
  for (size_t i = 0; i < saved_active_input_method_ids_.size(); ++i) {
    const std::string& input_method_id = saved_active_input_method_ids_[i];
    // Skip if it's not a keyboard layout. Drop input methods including
    // extension ones.
    if (!InputMethodUtil::IsKeyboardLayout(input_method_id))
      continue;
    active_input_method_ids_.push_back(input_method_id);
    if (input_method_id == hardware_keyboard_id)
      should_add_hardware_keyboard = false;
  }
  if (should_add_hardware_keyboard)
    active_input_method_ids_.push_back(hardware_keyboard_id);

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

bool InputMethodManagerImpl::ContainOnlyKeyboardLayout(
    const std::vector<std::string>& value) {
  for (size_t i = 0; i < value.size(); ++i) {
    if (!InputMethodUtil::IsKeyboardLayout(value[i]))
      return false;
  }
  return true;
}

void InputMethodManagerImpl::MaybeInitializeCandidateWindowController() {
  if (candidate_window_controller_.get())
    return;

  candidate_window_controller_.reset(
      CandidateWindowController::CreateCandidateWindowController());
  if (candidate_window_controller_->Init(ibus_controller_.get()))
    candidate_window_controller_->AddObserver(this);
  else
    DVLOG(1) << "Failed to initialize the candidate window controller";
}

}  // namespace input_method
}  // namespace chromeos
