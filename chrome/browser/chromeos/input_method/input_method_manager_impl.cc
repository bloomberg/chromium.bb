// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_manager_impl.h"

#include <algorithm>  // std::find

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/input_method/candidate_window_controller.h"
#include "chrome/browser/chromeos/input_method/component_extension_ime_manager_impl.h"
#include "chrome/browser/chromeos/input_method/input_method_engine_ibus.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/ibus/ibus_client.h"
#include "chromeos/dbus/ibus/ibus_input_context_client.h"
#include "chromeos/ime/component_extension_ime_manager.h"
#include "chromeos/ime/extension_ime_util.h"
#include "chromeos/ime/input_method_delegate.h"
#include "chromeos/ime/xkeyboard.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "ui/base/accelerators/accelerator.h"

namespace chromeos {
namespace input_method {

namespace {

const char nacl_mozc_us_id[] =
    "_comp_ime_fpfbhcjppmaeaijcidgiibchfbnhbeljnacl_mozc_us";
const char nacl_mozc_jp_id[] =
    "_comp_ime_fpfbhcjppmaeaijcidgiibchfbnhbeljnacl_mozc_jp";

bool Contains(const std::vector<std::string>& container,
              const std::string& value) {
  return std::find(container.begin(), container.end(), value) !=
      container.end();
}

const struct MigrationInputMethodList {
  const char* old_input_method;
  const char* new_input_method;
} kMigrationInputMethodList[] = {
  { "mozc", "_comp_ime_fpfbhcjppmaeaijcidgiibchfbnhbeljnacl_mozc_us" },
  { "mozc-jp", "_comp_ime_fpfbhcjppmaeaijcidgiibchfbnhbeljnacl_mozc_jp" },
  { "mozc-dv", "_comp_ime_fpfbhcjppmaeaijcidgiibchfbnhbeljnacl_mozc_us" },
  { "pinyin", "_comp_ime_nmblnjkfdkabgdofidlkienfnnbjhnabzh-t-i0-pinyin" },
  { "pinyin-dv", "_comp_ime_nmblnjkfdkabgdofidlkienfnnbjhnabzh-t-i0-pinyin" },
  { "mozc-chewing",
    "_comp_ime_ekbifjdfhkmdeeajnolmgdlmkllopefizh-hant-t-i0-und "},
  { "m17n:zh:cangjie",
    "_comp_ime_gjhclobljhjhgoebiipblnmdodbmpdgdzh-hant-t-i0-cangjie-1987" },
  { "_comp_ime_jcffnbbngddhenhcnebafkbdomehdhpdzh-t-i0-wubi-1986",
    "_comp_ime_gjhclobljhjhgoebiipblnmdodbmpdgdzh-t-i0-wubi-1986" },
  // TODO(nona): Remove following migration map in M31.
  { "m17n:ta:itrans",
    "_comp_ime_jhffeifommiaekmbkkjlpmilogcfdohpvkd_ta_itrans" },
  { "m17n:ta:tamil99",
    "_comp_ime_jhffeifommiaekmbkkjlpmilogcfdohpvkd_ta_tamil99" },
  { "m17n:ta:typewriter",
    "_comp_ime_jhffeifommiaekmbkkjlpmilogcfdohpvkd_ta_typewriter" },
  { "m17n:ta:inscript",
    "_comp_ime_jhffeifommiaekmbkkjlpmilogcfdohpvkd_ta_phone" },
  { "m17n:ta:phonetic",
    "_comp_ime_jhffeifommiaekmbkkjlpmilogcfdohpvkd_ta_inscript" },
  { "m17n:th:pattachote",
    "_comp_ime_jhffeifommiaekmbkkjlpmilogcfdohpvkd_th_pattajoti" },
  { "m17n:th:tis820", "_comp_ime_jhffeifommiaekmbkkjlpmilogcfdohpvkd_th_tis" },
  { "m17n:th:kesmanee",
    "_comp_ime_jhffeifommiaekmbkkjlpmilogcfdohpvkd_th" },
  { "m17n:vi:tcvn", "_comp_ime_jhffeifommiaekmbkkjlpmilogcfdohpvkd_vi_tcvn" },
  { "m17n:vi:viqr", "_comp_ime_jhffeifommiaekmbkkjlpmilogcfdohpvkd_vi_viqr" },
  { "m17n:vi:telex",
    "_comp_ime_jhffeifommiaekmbkkjlpmilogcfdohpvkd_vi_telex" },
  { "m17n:vi:vni",
    "_comp_ime_jhffeifommiaekmbkkjlpmilogcfdohpvkd_vi_vni" },
  { "m17n:am:sera",
    "_comp_ime_jhffeifommiaekmbkkjlpmilogcfdohpvkd_ethi" },
  { "m17n:bn:itrans",
    "_comp_ime_jhffeifommiaekmbkkjlpmilogcfdohpvkd_bn_phone" },
  { "m17n:gu:itrans",
    "_comp_ime_jhffeifommiaekmbkkjlpmilogcfdohpvkd_gu_phone" },
  { "m17n:hi:itrans",
    "_comp_ime_jhffeifommiaekmbkkjlpmilogcfdohpvkd_deva_phone" },
  { "m17n:kn:itrans",
    "_comp_ime_jhffeifommiaekmbkkjlpmilogcfdohpvkd_kn_phone" },
  { "m17n:ml:itrans",
    "_comp_ime_jhffeifommiaekmbkkjlpmilogcfdohpvkd_ml_phone" },
  { "m17n:mr:itrans",
    "_comp_ime_jhffeifommiaekmbkkjlpmilogcfdohpvkd_deva_phone" },
  { "m17n:te:itrans",
    "_comp_ime_jhffeifommiaekmbkkjlpmilogcfdohpvkd_te_phone" },
  { "m17n:fa:isiri", "_comp_ime_jhffeifommiaekmbkkjlpmilogcfdohpvkd_fa" },
  { "m17n:ar:kbd", "_comp_ime_jhffeifommiaekmbkkjlpmilogcfdohpvkd_ar" },
  // TODO(nona): Remove following migration map in M32
  { "m17n:zh:quick",
    "_comp_ime_ekbifjdfhkmdeeajnolmgdlmkllopefizh-hant-t-i0-und" },
};

const struct MigrationHangulKeyboardToInputMethodID {
  const char* keyboard_id;
  const char* ime_id;
} kMigrationHangulKeyboardToInputMethodID[] = {
  { "2", "_comp_ime_bdgdidmhaijohebebipajioienkglgfohangul_2set" },
  { "3f", "_comp_ime_bdgdidmhaijohebebipajioienkglgfohangul_3setfinal" },
  { "39", "_comp_ime_bdgdidmhaijohebebipajioienkglgfohangul_3set390" },
  { "3s", "_comp_ime_bdgdidmhaijohebebipajioienkglgfohangul_3setnoshift" },
  { "ro", "_comp_ime_bdgdidmhaijohebebipajioienkglgfohangul_romaja" },
};

}  // namespace

bool InputMethodManagerImpl::IsLoginKeyboard(
    const std::string& layout) const {
  const InputMethodDescriptor* ime =
      util_.GetInputMethodDescriptorFromId(layout);
  return ime ? ime->is_login_keyboard() : false;
}

InputMethodManagerImpl::InputMethodManagerImpl(
    scoped_ptr<InputMethodDelegate> delegate)
    : delegate_(delegate.Pass()),
      state_(STATE_LOGIN_SCREEN),
      util_(delegate_.get(), GetSupportedInputMethods()),
      component_extension_ime_manager_(new ComponentExtensionIMEManager()),
      weak_ptr_factory_(this) {
  IBusDaemonController::GetInstance()->AddObserver(this);
}

InputMethodManagerImpl::~InputMethodManagerImpl() {
  if (ibus_controller_.get())
    ibus_controller_->RemoveObserver(this);
  IBusDaemonController::GetInstance()->RemoveObserver(this);
  if (candidate_window_controller_.get()) {
    candidate_window_controller_->RemoveObserver(this);
    candidate_window_controller_->Shutdown();
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
      if (candidate_window_controller_.get()) {
        candidate_window_controller_->Shutdown();
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
      IsLoginKeyboard(initial_layout)) {
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
    if (!Contains(layouts, candidate) && IsLoginKeyboard(candidate))
      layouts.push_back(candidate);
  }

  active_input_method_ids_.swap(layouts);
  ChangeInputMethod(initial_layout);  // you can pass empty |initial_layout|.
}

// Adds new input method to given list.
bool InputMethodManagerImpl::EnableInputMethodImpl(
    const std::string& input_method_id,
    std::vector<std::string>& new_active_input_method_ids) const {
  if (!util_.IsValidInputMethodId(input_method_id)) {
    DVLOG(1) << "EnableInputMethod: Invalid ID: " << input_method_id;
    return false;
  }

  if (!Contains(new_active_input_method_ids, input_method_id))
    new_active_input_method_ids.push_back(input_method_id);

  return true;
}

// Starts or stops the system input method framework as needed.
void InputMethodManagerImpl::ReconfigureIMFramework() {
  if (component_extension_ime_manager_->IsInitialized())
    LoadNecessaryComponentExtensions();

  if (ContainsOnlyKeyboardLayout(active_input_method_ids_)) {
    // Do NOT call ibus_controller_->Stop(); here to work around a crash issue
    // at crbug.com/27051.
    // TODO(yusukes): We can safely call Stop(); here once crbug.com/26443
    // is implemented.
  } else {
    MaybeInitializeCandidateWindowController();
    IBusDaemonController::GetInstance()->Start();
  }
}

bool InputMethodManagerImpl::EnableInputMethod(
    const std::string& input_method_id) {
  if (!EnableInputMethodImpl(input_method_id, active_input_method_ids_))
    return false;

  ReconfigureIMFramework();
  return true;
}

bool InputMethodManagerImpl::EnableInputMethods(
    const std::vector<std::string>& new_active_input_method_ids) {
  if (state_ == STATE_TERMINATING)
    return false;

  // Filter unknown or obsolete IDs.
  std::vector<std::string> new_active_input_method_ids_filtered;

  for (size_t i = 0; i < new_active_input_method_ids.size(); ++i)
    EnableInputMethodImpl(new_active_input_method_ids[i],
                          new_active_input_method_ids_filtered);

  if (new_active_input_method_ids_filtered.empty()) {
    DVLOG(1) << "EnableInputMethods: No valid input method ID";
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

  ReconfigureIMFramework();

  // If |current_input_method| is no longer in |active_input_method_ids_|,
  // ChangeInputMethod() picks the first one in |active_input_method_ids_|.
  ChangeInputMethod(current_input_method_.id());
  return true;
}

bool InputMethodManagerImpl::MigrateOldInputMethods(
    std::vector<std::string>* input_method_ids) {
  bool rewritten = false;
  for (size_t i = 0; i < input_method_ids->size(); ++i) {
    for (size_t j = 0; j < ARRAYSIZE_UNSAFE(kMigrationInputMethodList); ++j) {
      if (input_method_ids->at(i) ==
          kMigrationInputMethodList[j].old_input_method) {
        input_method_ids->at(i).assign(
            kMigrationInputMethodList[j].new_input_method);
        rewritten = true;
      }
    }
  }
  std::vector<std::string>::iterator it =
      std::unique(input_method_ids->begin(), input_method_ids->end());
  input_method_ids->resize(std::distance(input_method_ids->begin(), it));
  return rewritten;
}

bool InputMethodManagerImpl::MigrateKoreanKeyboard(
    const std::string& keyboard_id,
    std::vector<std::string>* input_method_ids) {
  std::vector<std::string>::iterator it =
      std::find(active_input_method_ids_.begin(),
                active_input_method_ids_.end(),
                "mozc-hangul");
  if (it == active_input_method_ids_.end())
    return false;

  for (size_t i = 0;
       i < ARRAYSIZE_UNSAFE(kMigrationHangulKeyboardToInputMethodID); ++i) {
    if (kMigrationHangulKeyboardToInputMethodID[i].keyboard_id == keyboard_id) {
      *it = kMigrationHangulKeyboardToInputMethodID[i].ime_id;
      input_method_ids->assign(active_input_method_ids_.begin(),
                               active_input_method_ids_.end());
      return true;
    }
  }
  return false;
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

  if (!component_extension_ime_manager_->IsInitialized() ||
      (!InputMethodUtil::IsKeyboardLayout(input_method_id_to_switch) &&
       !IsIBusConnectionAlive())) {
    // We can't change input method before the initialization of component
    // extension ime manager or before connection to ibus-daemon is not
    // established. ChangeInputMethod will be called with
    // |pending_input_method_| when the both initialization is done.
    pending_input_method_ = input_method_id_to_switch;
    return false;
  }

  pending_input_method_.clear();
  IBusInputContextClient* input_context =
      chromeos::DBusThreadManager::Get()->GetIBusInputContextClient();
  const std::string current_input_method_id = current_input_method_.id();
  IBusClient* client = DBusThreadManager::Get()->GetIBusClient();
  if (InputMethodUtil::IsKeyboardLayout(input_method_id_to_switch)) {
    FOR_EACH_OBSERVER(InputMethodManager::Observer,
                      observers_,
                      InputMethodPropertyChanged(this));
    // Hack for fixing http://crosbug.com/p/12798
    // We should notify IME switching to ibus-daemon, otherwise
    // IBusPreeditFocusMode does not work. To achieve it, change engine to
    // itself if the next engine is XKB layout.
    if (current_input_method_id.empty() ||
        InputMethodUtil::IsKeyboardLayout(current_input_method_id)) {
      if (input_context)
        input_context->Reset();
    } else {
      if (client)
        client->SetGlobalEngine(current_input_method_id,
                                base::Bind(&base::DoNothing));
    }
    if (input_context)
      input_context->SetIsXKBLayout(true);
  } else {
    DCHECK(client);
    client->SetGlobalEngine(input_method_id_to_switch,
                            base::Bind(&base::DoNothing));
    if (input_context)
      input_context->SetIsXKBLayout(false);
  }

  if (current_input_method_id != input_method_id_to_switch) {
    // Clear input method properties unconditionally if
    // |input_method_id_to_switch| is not equal to |current_input_method_id|.
    //
    // When switching to another input method and no text area is focused,
    // RegisterProperties signal for the new input method will NOT be sent
    // until a text area is focused. Therefore, we have to clear the old input
    // method properties here to keep the input method switcher status
    // consistent.
    //
    // When |input_method_id_to_switch| and |current_input_method_id| are the
    // same, the properties shouldn't be cleared. If we do that, something
    // wrong happens in step #4 below:
    // 1. Enable "xkb:us::eng" and "mozc". Switch to "mozc".
    // 2. Focus Omnibox. IME properties for mozc are sent to Chrome.
    // 3. Switch to "xkb:us::eng". No function in this file is called.
    // 4. Switch back to "mozc". ChangeInputMethod("mozc") is called, but it's
    //    basically NOP since ibus-daemon's current IME is already "mozc".
    //    IME properties are not sent to Chrome for the same reason.
    // TODO(nona): Revisit above comment once ibus-daemon is gone.
    ibus_controller_->ClearProperties();

    const InputMethodDescriptor* descriptor = NULL;
    if (!extension_ime_util::IsExtensionIME(input_method_id_to_switch)) {
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

void InputMethodManagerImpl::OnComponentExtensionInitialized(
    scoped_ptr<ComponentExtensionIMEManagerDelegate> delegate) {
  DCHECK(thread_checker_.CalledOnValidThread());
  component_extension_ime_manager_->Initialize(delegate.Pass());
  util_.SetComponentExtensions(
      component_extension_ime_manager_->GetAllIMEAsInputMethodDescriptor());

  LoadNecessaryComponentExtensions();

  if (!pending_input_method_.empty())
    ChangeInputMethodInternal(pending_input_method_, false);

}

void InputMethodManagerImpl::LoadNecessaryComponentExtensions() {
  if (!component_extension_ime_manager_->IsInitialized())
    return;
  // Load component extensions but also update |active_input_method_ids_| as
  // some component extension IMEs may have been removed from the Chrome OS
  // image. If specified component extension IME no longer exists, falling back
  // to an existing IME.
  std::vector<std::string> unfiltered_input_method_ids =
      active_input_method_ids_;
  active_input_method_ids_.clear();
  for (size_t i = 0; i < unfiltered_input_method_ids.size(); ++i) {
    if (!component_extension_ime_manager_->IsComponentExtensionIMEId(
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

void InputMethodManagerImpl::ActivateInputMethodProperty(
    const std::string& key) {
  DCHECK(!key.empty());
  ibus_controller_->ActivateInputMethodProperty(key);
}

void InputMethodManagerImpl::AddInputMethodExtension(
    const std::string& id,
    const std::string& name,
    const std::vector<std::string>& layouts,
    const std::vector<std::string>& languages,
    const GURL& options_url,
    InputMethodEngine* engine) {
  if (state_ == STATE_TERMINATING)
    return;

  if (!extension_ime_util::IsExtensionIME(id) &&
      !ComponentExtensionIMEManager::IsComponentExtensionIMEId(id)) {
    DVLOG(1) << id << " is not a valid extension input method ID.";
    return;
  }

  extra_input_methods_[id] =
      InputMethodDescriptor(id, name, layouts, languages, false, options_url);
  if (Contains(enabled_extension_imes_, id) &&
      !ComponentExtensionIMEManager::IsComponentExtensionIMEId(id)) {
    if (!Contains(active_input_method_ids_, id)) {
      active_input_method_ids_.push_back(id);
    } else {
      DVLOG(1) << "AddInputMethodExtension: alread added: "
               << id << ", " << name;
      // Call Start() anyway, just in case.
    }

    // Ensure that the input method daemon is running.
    MaybeInitializeCandidateWindowController();
    IBusDaemonController::GetInstance()->Start();
  }

  extra_input_method_instances_[id] =
      static_cast<InputMethodEngineIBus*>(engine);
}

void InputMethodManagerImpl::RemoveInputMethodExtension(const std::string& id) {
  if (!extension_ime_util::IsExtensionIME(id))
    DVLOG(1) << id << " is not a valid extension input method ID.";

  std::vector<std::string>::iterator i = std::find(
      active_input_method_ids_.begin(), active_input_method_ids_.end(), id);
  if (i != active_input_method_ids_.end())
    active_input_method_ids_.erase(i);
  extra_input_methods_.erase(id);

  if (ContainsOnlyKeyboardLayout(active_input_method_ids_)) {
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
    if (ComponentExtensionIMEManager::IsComponentExtensionIMEId(
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
    IBusDaemonController::GetInstance()->Start();

    // If |current_input_method| is no longer in |active_input_method_ids_|,
    // switch to the first one in |active_input_method_ids_|.
    ChangeInputMethod(current_input_method_.id());
  }
}

void InputMethodManagerImpl::SetInputMethodDefault() {
  // Set up keyboards. For example, when |locale| is "en-US", enable US qwerty
  // and US dvorak keyboard layouts.
  if (g_browser_process && g_browser_process->local_state()) {
    const std::string locale = g_browser_process->GetApplicationLocale();
    // If the preferred keyboard for the login screen has been saved, use it.
    PrefService* prefs = g_browser_process->local_state();
    std::string initial_input_method_id =
        prefs->GetString(chromeos::language_prefs::kPreferredKeyboardLayout);
    if (initial_input_method_id.empty()) {
      // If kPreferredKeyboardLayout is not specified, use the hardware layout.
      initial_input_method_id =
          GetInputMethodUtil()->GetHardwareInputMethodId();
    }
    EnableLayouts(locale, initial_input_method_id);
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
      input_method_ids_to_switch.push_back(nacl_mozc_jp_id);
      break;
    case ui::VKEY_NONCONVERT:  // Muhenkan key on JP106 keyboard
      input_method_ids_to_switch.push_back("xkb:jp::jpn");
      break;
    case ui::VKEY_DBE_SBCSCHAR:  // ZenkakuHankaku key on JP106 keyboard
    case ui::VKEY_DBE_DBCSCHAR:
      input_method_ids_to_switch.push_back(nacl_mozc_jp_id);
      input_method_ids_to_switch.push_back("xkb:jp::jpn");
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

ComponentExtensionIMEManager*
    InputMethodManagerImpl::GetComponentExtensionIMEManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return component_extension_ime_manager_.get();
}

void InputMethodManagerImpl::OnConnected() {
  for (std::map<std::string, InputMethodEngineIBus*>::iterator ite =
          extra_input_method_instances_.begin();
       ite != extra_input_method_instances_.end();
       ite++) {
    if (Contains(enabled_extension_imes_, ite->first) ||
        (component_extension_ime_manager_->IsInitialized() &&
         component_extension_ime_manager_->IsWhitelisted(ite->first))) {
      ite->second->OnConnected();
    }
  }

  if (!pending_input_method_.empty())
    ChangeInputMethodInternal(pending_input_method_, false);
}

void InputMethodManagerImpl::OnDisconnected() {
  for (std::map<std::string, InputMethodEngineIBus*>::iterator ite =
          extra_input_method_instances_.begin();
       ite != extra_input_method_instances_.end();
       ite++) {
    if (Contains(enabled_extension_imes_, ite->first))
      ite->second->OnDisconnected();
  }
}

void InputMethodManagerImpl::InitializeComponentExtension() {
  ComponentExtensionIMEManagerImpl* impl =
      new ComponentExtensionIMEManagerImpl();
  scoped_ptr<ComponentExtensionIMEManagerDelegate> delegate(impl);
  impl->InitializeAsync(base::Bind(
                       &InputMethodManagerImpl::OnComponentExtensionInitialized,
                       weak_ptr_factory_.GetWeakPtr(),
                       base::Passed(&delegate)));
}

void InputMethodManagerImpl::Init(base::SequencedTaskRunner* ui_task_runner) {
  DCHECK(!ibus_controller_.get());
  DCHECK(thread_checker_.CalledOnValidThread());

  ibus_controller_.reset(IBusController::Create());
  xkeyboard_.reset(XKeyboard::Create());
  ibus_controller_->AddObserver(this);

  // We can't call impl->Initialize here, because file thread is not available
  // at this moment.
  ui_task_runner->PostTask(
      FROM_HERE,
      base::Bind(&InputMethodManagerImpl::InitializeComponentExtension,
                 weak_ptr_factory_.GetWeakPtr()));
}

void InputMethodManagerImpl::SetIBusControllerForTesting(
    IBusController* ibus_controller) {
  ibus_controller_.reset(ibus_controller);
  ibus_controller_->AddObserver(this);
}

void InputMethodManagerImpl::SetCandidateWindowControllerForTesting(
    CandidateWindowController* candidate_window_controller) {
  candidate_window_controller_.reset(candidate_window_controller);
  candidate_window_controller_->Init();
  candidate_window_controller_->AddObserver(this);
}

void InputMethodManagerImpl::SetXKeyboardForTesting(XKeyboard* xkeyboard) {
  xkeyboard_.reset(xkeyboard);
}

void InputMethodManagerImpl::InitializeComponentExtensionForTesting(
    scoped_ptr<ComponentExtensionIMEManagerDelegate> delegate) {
  OnComponentExtensionInitialized(delegate.Pass());
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
    if (!IsLoginKeyboard(input_method_id))
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

bool InputMethodManagerImpl::ContainsOnlyKeyboardLayout(
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
  if (candidate_window_controller_->Init())
    candidate_window_controller_->AddObserver(this);
  else
    DVLOG(1) << "Failed to initialize the candidate window controller";
}

bool InputMethodManagerImpl::IsIBusConnectionAlive() {
  return DBusThreadManager::Get() && DBusThreadManager::Get()->GetIBusClient();
}

}  // namespace input_method
}  // namespace chromeos
