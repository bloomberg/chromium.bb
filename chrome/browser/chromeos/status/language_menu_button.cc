// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/language_menu_button.h"

#include <string>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/keyboard_library.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/status/status_area_host.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

// The language menu consists of 3 parts (in this order):
//
//   (1) input method names. The size of the list is always >= 1.
//   (2) input method properties. This list might be empty.
//   (3) "Customize language and input..." button.
//
// Example of the menu (Japanese):
//
// ============================== (border of the popup window)
// [ ] English                    (|index| in the following functions is 0)
// [*] Japanese
// [ ] Chinese (Simplified)
// ------------------------------ (separator)
// [*] Hiragana                   (index = 5, The property has 2 radio groups)
// [ ] Katakana
// [ ] HalfWidthKatakana
// [*] Roman
// [ ] Kana
// ------------------------------ (separator)
// Customize language and input...(index = 11)
// ============================== (border of the popup window)
//
// Example of the menu (Simplified Chinese):
//
// ============================== (border of the popup window)
// [ ] English
// [ ] Japanese
// [*] Chinese (Simplified)
// ------------------------------ (separator)
// Switch to full letter mode     (The property has 2 command buttons)
// Switch to half punctuation mode
// ------------------------------ (separator)
// Customize language and input...
// ============================== (border of the popup window)
//

namespace {

// Constants to specify the type of items in |model_|.
enum {
  COMMAND_ID_INPUT_METHODS = 0,  // English, Chinese, Japanese, Arabic, ...
  COMMAND_ID_IME_PROPERTIES,  // Hiragana, Katakana, ...
  COMMAND_ID_CUSTOMIZE_LANGUAGE,  // "Customize language and input..." button.
};

// A group ID for IME properties starts from 0. We use the huge value for the
// input method list to avoid conflict.
const int kRadioGroupLanguage = 1 << 16;
const int kRadioGroupNone = -1;

// A mapping from an input method id to a text for the language indicator. The
// mapping is necessary since some input methods belong to the same language.
// For example, both "xkb:us::eng" and "xkb:us:dvorak:eng" are for US English.
const struct {
  const char* input_method_id;
  const char* indicator_text;
} kMappingFromIdToIndicatorText[] = {
  // To distinguish from "xkb:us::eng"
  { "xkb:us:dvorak:eng", "DV" },
  // To distinguish from "xkb:jp::jpn"
  { "mozc", "\xe3\x81\x82" },  // Japanese Hiragana letter A in UTF-8.
  { "mozc-dv", "\xe3\x81\x82" },
  { "mozc-jp", "\xe3\x81\x82" },
  // For simplified Chinese input methods
  { "pinyin", "\xe6\x8b\xbc" },  // U+62FC
  // For traditional Chinese input methods
  { "chewing", "\xe9\x85\xb7" },  // U+9177
  { "m17n:zh:cangjie", "\xe5\x80\x89" },  // U+5009
  { "m17n:zh:quick", "\xe9\x80\x9f" },  // U+901F
  // For Hangul input method.
  { "hangul", "\xea\xb0\x80" },  // U+AC00
};
const size_t kMappingFromIdToIndicatorTextLen =
    ARRAYSIZE_UNSAFE(kMappingFromIdToIndicatorText);

// Returns the language name for the given |language_code|.
std::wstring GetLanguageName(const std::string& language_code) {
  const string16 language_name = l10n_util::GetDisplayNameForLocale(
      language_code, g_browser_process->GetApplicationLocale(), true);
  return UTF16ToWide(language_name);
}

// Returns PrefService object associated with |host|. Returns NULL if we are NOT
// within a browser.
PrefService* GetPrefService(chromeos::StatusAreaHost* host) {
  if (host->GetProfile()) {
    return host->GetProfile()->GetPrefs();
  }
  return NULL;
}

}  // namespace

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// LanguageMenuButton

LanguageMenuButton::LanguageMenuButton(StatusAreaHost* host)
    : StatusAreaButton(this),
      input_method_descriptors_(CrosLibrary::Get()->GetInputMethodLibrary()->
                                GetActiveInputMethods()),
      model_(NULL),
      // Be aware that the constructor of |language_menu_| calls GetItemCount()
      // in this class. Therefore, GetItemCount() have to return 0 when
      // |model_| is NULL.
      ALLOW_THIS_IN_INITIALIZER_LIST(language_menu_(this)),
      host_(host),
      logged_in_(false) {
  DCHECK(input_method_descriptors_.get() &&
         !input_method_descriptors_->empty());
  set_border(NULL);
  set_use_menu_button_paint(true);
  SetFont(ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::BaseFont).DeriveFont(1, gfx::Font::BOLD));
  SetEnabledColor(0xB3FFFFFF);  // White with 70% Alpha
  SetDisabledColor(0x00FFFFFF);  // White with 00% Alpha (invisible)
  SetShowMultipleIconStates(false);
  set_alignment(TextButton::ALIGN_CENTER);

  // Update the model
  RebuildModel();

  // Draw the default indicator "US". The default indicator "US" is used when
  // |pref_service| is not available (for example, unit tests) or |pref_service|
  // is available, but Chrome preferences are not available (for example,
  // initial OS boot).
  UpdateIndicator(L"US", L"");

  // Use the same keyboard layout on all windows.
  CrosLibrary::Get()->GetKeyboardLibrary()->SetKeyboardLayoutPerWindow(false);

  // Sync current and previous input methods on Chrome prefs with ibus-daemon.
  // InputMethodChanged() will be called soon and the indicator will be updated.
  InputMethodLibrary* library = CrosLibrary::Get()->GetInputMethodLibrary();
  PrefService* pref_service = GetPrefService(host_);
  if (pref_service && host_->IsBrowserMode()) {
    previous_input_method_pref_.Init(
        prefs::kLanguagePreviousInputMethod, pref_service, this);
    const std::string previous_input_method_id =
        previous_input_method_pref_.GetValue();
    if (!previous_input_method_id.empty())
      library->ChangeInputMethod(previous_input_method_id);

    current_input_method_pref_.Init(
        prefs::kLanguageCurrentInputMethod, pref_service, this);
    const std::string current_input_method_id =
        current_input_method_pref_.GetValue();
    if (!current_input_method_id.empty())
      library->ChangeInputMethod(current_input_method_id);
  }
  library->AddObserver(this);

  if (host_->IsBrowserMode() || host_->IsScreenLockerMode()) {
    logged_in_ = true;
  }
  if (!logged_in_) {
    registrar_.Add(this,
                   NotificationType::LOGIN_USER_CHANGED,
                   NotificationService::AllSources());
  }
}

LanguageMenuButton::~LanguageMenuButton() {
  CrosLibrary::Get()->GetInputMethodLibrary()->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// LanguageMenuButton, menus::MenuModel implementation:

int LanguageMenuButton::GetCommandIdAt(int index) const {
  return 0;  // dummy
}

bool LanguageMenuButton::IsLabelDynamicAt(int index) const {
  // Menu content for the language button could change time by time.
  return true;
}

bool LanguageMenuButton::GetAcceleratorAt(
    int index, menus::Accelerator* accelerator) const {
  // Views for Chromium OS does not support accelerators yet.
  return false;
}

bool LanguageMenuButton::IsItemCheckedAt(int index) const {
  DCHECK_GE(index, 0);
  DCHECK(input_method_descriptors_.get());

  if (IndexIsInInputMethodList(index)) {
    const InputMethodDescriptor& input_method
        = input_method_descriptors_->at(index);
    return input_method == CrosLibrary::Get()->GetInputMethodLibrary()->
          current_input_method();
  }

  if (GetPropertyIndex(index, &index)) {
    const ImePropertyList& property_list
        = CrosLibrary::Get()->GetInputMethodLibrary()->current_ime_properties();
    return property_list.at(index).is_selection_item_checked;
  }

  // Separator(s) or the "Customize language and input..." button.
  return false;
}

int LanguageMenuButton::GetGroupIdAt(int index) const {
  DCHECK_GE(index, 0);

  if (IndexIsInInputMethodList(index)) {
    return kRadioGroupLanguage;
  }

  if (GetPropertyIndex(index, &index)) {
    const ImePropertyList& property_list
        = CrosLibrary::Get()->GetInputMethodLibrary()->current_ime_properties();
    return property_list.at(index).selection_item_id;
  }

  return kRadioGroupNone;
}

bool LanguageMenuButton::HasIcons() const  {
  // We don't support icons on Chrome OS.
  return false;
}

bool LanguageMenuButton::GetIconAt(int index, SkBitmap* icon) const {
  return false;
}

menus::ButtonMenuItemModel* LanguageMenuButton::GetButtonMenuItemAt(
    int index) const {
  return NULL;
}

bool LanguageMenuButton::IsEnabledAt(int index) const {
  // Just return true so all input method names and input method propertie names
  // could be clicked.
  return true;
}

menus::MenuModel* LanguageMenuButton::GetSubmenuModelAt(int index) const {
  // We don't use nested menus.
  return NULL;
}

void LanguageMenuButton::HighlightChangedTo(int index) {
  // Views for Chromium OS does not support this interface yet.
}

void LanguageMenuButton::MenuWillShow() {
  // Views for Chromium OS does not support this interface yet.
}

int LanguageMenuButton::GetItemCount() const {
  if (!model_.get()) {
    // Model is not constructed yet. This means that LanguageMenuButton is
    // being constructed. Return zero.
    return 0;
  }
  return model_->GetItemCount();
}

menus::MenuModel::ItemType LanguageMenuButton::GetTypeAt(int index) const {
  DCHECK_GE(index, 0);

  if (IndexPointsToConfigureImeMenuItem(index)) {
    return menus::MenuModel::TYPE_COMMAND;  // "Customize language and input"
  }

  if (IndexIsInInputMethodList(index)) {
    return menus::MenuModel::TYPE_RADIO;
  }

  if (GetPropertyIndex(index, &index)) {
    const ImePropertyList& property_list
        = CrosLibrary::Get()->GetInputMethodLibrary()->current_ime_properties();
    if (property_list.at(index).is_selection_item) {
      return menus::MenuModel::TYPE_RADIO;
    }
    return menus::MenuModel::TYPE_COMMAND;
  }

  return menus::MenuModel::TYPE_SEPARATOR;
}

string16 LanguageMenuButton::GetLabelAt(int index) const {
  DCHECK_GE(index, 0);
  DCHECK(input_method_descriptors_.get());

  // We use IDS_OPTIONS_SETTINGS_LANGUAGES_CUSTOMIZE here as the button
  // opens the same dialog that is opened from the main options dialog.
  if (IndexPointsToConfigureImeMenuItem(index)) {
    return l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_LANGUAGES_CUSTOMIZE);
  }

  std::wstring name;
  if (IndexIsInInputMethodList(index)) {
    name = GetTextForMenu(input_method_descriptors_->at(index));
  } else if (GetPropertyIndex(index, &index)) {
    const ImePropertyList& property_list
        = CrosLibrary::Get()->GetInputMethodLibrary()->current_ime_properties();
    return input_method::GetStringUTF16(property_list.at(index).label);
  }

  return WideToUTF16(name);
}

void LanguageMenuButton::ActivatedAt(int index) {
  DCHECK_GE(index, 0);
  DCHECK(input_method_descriptors_.get());

  if (IndexPointsToConfigureImeMenuItem(index)) {
    host_->OpenButtonOptions(this);
    return;
  }

  if (IndexIsInInputMethodList(index)) {
    // Inter-IME switching.
    const InputMethodDescriptor& input_method
        = input_method_descriptors_->at(index);
    CrosLibrary::Get()->GetInputMethodLibrary()->ChangeInputMethod(
        input_method.id);
    return;
  }

  if (GetPropertyIndex(index, &index)) {
    // Intra-IME switching (e.g. Japanese-Hiragana to Japanese-Katakana).
    const ImePropertyList& property_list
        = CrosLibrary::Get()->GetInputMethodLibrary()->current_ime_properties();
    const std::string key = property_list.at(index).key;
    if (property_list.at(index).is_selection_item) {
      // Radio button is clicked.
      const int id = property_list.at(index).selection_item_id;
      // First, deactivate all other properties in the same radio group.
      for (int i = 0; i < static_cast<int>(property_list.size()); ++i) {
        if (i != index && id == property_list.at(i).selection_item_id) {
          CrosLibrary::Get()->GetInputMethodLibrary()->SetImePropertyActivated(
              property_list.at(i).key, false);
        }
      }
      // Then, activate the property clicked.
      CrosLibrary::Get()->GetInputMethodLibrary()->SetImePropertyActivated(
          key, true);
    } else {
      // Command button like "Switch to half punctuation mode" is clicked.
      // We can always use "Deactivate" for command buttons.
      CrosLibrary::Get()->GetInputMethodLibrary()->SetImePropertyActivated(
          key, false);
    }
    return;
  }

  LOG(ERROR) << "Unexpected index: " << index;
}

////////////////////////////////////////////////////////////////////////////////
// LanguageMenuButton, views::ViewMenuDelegate implementation:

void LanguageMenuButton::RunMenu(views::View* source, const gfx::Point& pt) {
  UserMetrics::RecordAction(
      UserMetricsAction("LanguageMenuButton_Open"));
  input_method_descriptors_.reset(CrosLibrary::Get()->GetInputMethodLibrary()->
                                  GetActiveInputMethods());
  RebuildModel();
  language_menu_.Rebuild();

  // Disallow the menu widget to grab the keyboard focus. This is necessary to
  // enable users to change status of an input method (e.g. change the input
  // mode from Japanese Hiragana to Japanese Katakana) without discarding a
  // preedit string. See crosbug.com/5796 for details. Note that menus other
  // than this one should not call the Gtk+ API since it is a special API only
  // for a menu related to IME/keyboard. See the Gtk+ API reference at:
  // http://library.gnome.org/devel/gtk/stable/GtkMenuShell.html
  gfx::NativeMenu native_menu = language_menu_.GetNativeMenu();
  if (native_menu) {
    gtk_menu_shell_set_take_focus(GTK_MENU_SHELL(native_menu), FALSE);
  } else {
    LOG(ERROR)
        << "Can't call gtk_menu_shell_set_take_focus since NativeMenu is NULL";
  }

  language_menu_.UpdateStates();
  language_menu_.RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
}

////////////////////////////////////////////////////////////////////////////////
// InputMethodLibrary::Observer implementation:

void LanguageMenuButton::InputMethodChanged(InputMethodLibrary* obj) {
  UserMetrics::RecordAction(
      UserMetricsAction("LanguageMenuButton_InputMethodChanged"));

  const InputMethodDescriptor& previous_input_method =
      obj->previous_input_method();
  const InputMethodDescriptor& current_input_method =
      obj->current_input_method();
  UpdateIndicatorFromInputMethod(current_input_method);
  // Update Chrome prefs as well.
  if (host_->IsBrowserMode()) {
    if (GetPrefService(host_)) {  // make sure we're not in unit tests.
      // Sometimes (e.g. initial boot) |previous_input_method.id| is empty.
      previous_input_method_pref_.SetValue(previous_input_method.id);
      current_input_method_pref_.SetValue(current_input_method.id);
    }
  } else {
    // We're in the login screen (i.e. not in the normal browser mode nor screen
    // locker mode). If a user has already logged in, we should not update the
    // local state since a profile for the user might be loaded before the
    // buttun for the login screen is destroyed.
    if (!logged_in_ && g_browser_process && g_browser_process->local_state()) {
        g_browser_process->local_state()->SetString(
            language_prefs::kPreferredKeyboardLayout, current_input_method.id);
        g_browser_process->local_state()->SavePersistentPrefs();
    }
  }
}

void LanguageMenuButton::ActiveInputMethodsChanged(InputMethodLibrary* obj) {
  // Update the icon if active input methods are changed. See also
  // comments in UpdateIndicator()
  UpdateIndicatorFromInputMethod(obj->current_input_method());
}

void LanguageMenuButton::ImePropertiesChanged(InputMethodLibrary* obj) {
}

////////////////////////////////////////////////////////////////////////////////
// views::View implementation:

void LanguageMenuButton::OnLocaleChanged() {
  input_method::OnLocaleChanged();
  const InputMethodDescriptor& input_method =
      CrosLibrary::Get()->GetInputMethodLibrary()->current_input_method();
  UpdateIndicatorFromInputMethod(input_method);
  Layout();
  SchedulePaint();
}

void LanguageMenuButton::UpdateIndicator(
    const std::wstring& name, const std::wstring& tooltip) {
  // Hide the button only if there is only one input method, and the input
  // method is a XKB keyboard layout. We don't hide the button for other
  // types of input methods as these might have intra input method modes,
  // like Hiragana and Katakana modes in Japanese input methods.
  scoped_ptr<InputMethodDescriptors> active_input_methods(
      CrosLibrary::Get()->GetInputMethodLibrary()->GetActiveInputMethods());
  if (active_input_methods->size() == 1 &&
      input_method::IsKeyboardLayout(active_input_methods->at(0).id) &&
      host_->IsBrowserMode()) {
    // As the disabled color is set to invisible, disabling makes the
    // button disappear.
    SetEnabled(false);
    SetTooltipText(L"");  // remove tooltip
  } else {
    SetEnabled(true);
    SetTooltipText(tooltip);
  }
  SetText(name);
  SchedulePaint();
}

void LanguageMenuButton::UpdateIndicatorFromInputMethod(
    const InputMethodDescriptor& input_method) {
  const std::wstring name = GetTextForIndicator(input_method);
  const std::wstring tooltip = GetTextForMenu(input_method);
  UpdateIndicator(name, tooltip);
}

void LanguageMenuButton::RebuildModel() {
  model_.reset(new menus::SimpleMenuModel(NULL));
  string16 dummy_label = UTF8ToUTF16("");
  // Indicates if separator's needed before each section.
  bool need_separator = false;

  if (!input_method_descriptors_->empty()) {
    // We "abuse" the command_id and group_id arguments of AddRadioItem method.
    // A COMMAND_ID_XXX enum value is passed as command_id, and array index of
    // |input_method_descriptors_| or |property_list| is passed as group_id.
    for (size_t i = 0; i < input_method_descriptors_->size(); ++i) {
      model_->AddRadioItem(COMMAND_ID_INPUT_METHODS, dummy_label, i);
    }

    need_separator = true;
  }

  const ImePropertyList& property_list
      = CrosLibrary::Get()->GetInputMethodLibrary()->current_ime_properties();
  if (!property_list.empty()) {
    if (need_separator)
      model_->AddSeparator();
    for (size_t i = 0; i < property_list.size(); ++i) {
      model_->AddRadioItem(COMMAND_ID_IME_PROPERTIES, dummy_label, i);
    }
    need_separator = true;
  }

  if (host_->ShouldOpenButtonOptions(this)) {
    // Note: We use AddSeparator() for separators, and AddRadioItem() for all
    // other items even if an item is not actually a radio item.
    if (need_separator)
      model_->AddSeparator();
    model_->AddRadioItem(COMMAND_ID_CUSTOMIZE_LANGUAGE, dummy_label,
                         0 /* dummy */);
  }
}

bool LanguageMenuButton::IndexIsInInputMethodList(int index) const {
  DCHECK_GE(index, 0);
  DCHECK(model_.get());
  if (index >= model_->GetItemCount()) {
    return false;
  }

  return ((model_->GetTypeAt(index) == menus::MenuModel::TYPE_RADIO) &&
          (model_->GetCommandIdAt(index) == COMMAND_ID_INPUT_METHODS) &&
          input_method_descriptors_.get() &&
          (index < static_cast<int>(input_method_descriptors_->size())));
}

bool LanguageMenuButton::GetPropertyIndex(
    int index, int* property_index) const {
  DCHECK_GE(index, 0);
  DCHECK(property_index);
  DCHECK(model_.get());
  if (index >= model_->GetItemCount()) {
    return false;
  }

  if ((model_->GetTypeAt(index) == menus::MenuModel::TYPE_RADIO) &&
      (model_->GetCommandIdAt(index) == COMMAND_ID_IME_PROPERTIES)) {
    const int tmp_property_index = model_->GetGroupIdAt(index);
    const ImePropertyList& property_list
        = CrosLibrary::Get()->GetInputMethodLibrary()->current_ime_properties();
    if (tmp_property_index < static_cast<int>(property_list.size())) {
      *property_index = tmp_property_index;
      return true;
    }
  }
  return false;
}

bool LanguageMenuButton::IndexPointsToConfigureImeMenuItem(int index) const {
  DCHECK_GE(index, 0);
  DCHECK(model_.get());
  if (index >= model_->GetItemCount()) {
    return false;
  }

  return ((model_->GetTypeAt(index) == menus::MenuModel::TYPE_RADIO) &&
          (model_->GetCommandIdAt(index) == COMMAND_ID_CUSTOMIZE_LANGUAGE));
}

std::wstring LanguageMenuButton::GetTextForIndicator(
    const InputMethodDescriptor& input_method) {
  // For the status area, we use two-letter, upper-case language code like
  // "US" and "JP".
  std::wstring text;

  // Check special cases first.
  for (size_t i = 0; i < kMappingFromIdToIndicatorTextLen; ++i) {
    if (kMappingFromIdToIndicatorText[i].input_method_id == input_method.id) {
      text = UTF8ToWide(kMappingFromIdToIndicatorText[i].indicator_text);
      break;
    }
  }

  // Display the keyboard layout name when using ibus-xkb.
  if (text.empty()) {
    const size_t kMaxKeyboardLayoutNameLen = 2;
    const std::wstring keyboard_layout = UTF8ToWide(
        input_method::GetKeyboardLayoutName(input_method.id));
    text = StringToUpperASCII(keyboard_layout).substr(
        0, kMaxKeyboardLayoutNameLen);
  }

  // TODO(yusukes): Some languages have two or more input methods. For example,
  // Thai has 3, Vietnamese has 4. If these input methods could be activated at
  // the same time, we should do either of the following:
  //   (1) Add mappings to |kMappingFromIdToIndicatorText|
  //   (2) Add suffix (1, 2, ...) to |text| when ambiguous.

  if (text.empty()) {
    const size_t kMaxLanguageNameLen = 2;
    std::string language_code =
        input_method::GetLanguageCodeFromDescriptor(input_method);

    // Use "CN" for simplified Chinese and "TW" for traditonal Chinese,
    // rather than "ZH".
    if (StartsWithASCII(language_code, "zh-", false)) {
      std::vector<std::string> portions;
      SplitString(language_code, '-', &portions);
      if (portions.size() >= 2 && !portions[1].empty()) {
        language_code = portions[1];
      }
    }

    text = StringToUpperASCII(UTF8ToWide(language_code)).substr(
        0, kMaxLanguageNameLen);
  }
  DCHECK(!text.empty());
  return text;
}

std::wstring LanguageMenuButton::GetTextForMenu(
    const InputMethodDescriptor& input_method) {
  // We don't show language here.  Name of keyboard layout or input method
  // usually imply (or explicitly include) its language.

  // Special case for Dutch, French and German: these languages have multiple
  // keyboard layouts and share the same laout of keyboard (Belgian). We need to
  // show explicitly the language for the layout.
  // For Arabic and Hindi: they share "Standard Input Method".
  const std::string language_code
      = input_method::GetLanguageCodeFromDescriptor(input_method);
  std::wstring text;
  if (language_code == "ar" ||
      language_code == "hi" ||
      language_code == "nl" ||
      language_code == "fr" ||
      language_code == "de") {
    text = GetLanguageName(language_code) + L" - ";
  }
  text += input_method::GetString(input_method.display_name);

  DCHECK(!text.empty());
  return text;
}

void LanguageMenuButton::RegisterPrefs(PrefService* local_state) {
  local_state->RegisterStringPref(language_prefs::kPreferredKeyboardLayout,
                                  "");
}

void LanguageMenuButton::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  if (type == NotificationType::LOGIN_USER_CHANGED) {
    logged_in_ = true;
  }
}

}  // namespace chromeos
