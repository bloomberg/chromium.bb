// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/language_menu_button.h"

#include <string>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/time.h"
#include "chrome/browser/chromeos/status_area_host.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

// The language menu consists of 3 parts (in this order):
//
//   (1) XKB layout names and IME languages names. The size of the list is
//       always >= 1.
//   (2) IME properties. This list might be empty.
//   (3) "Configure IME..." button.
//
// Example of the menu (Japanese):
//
// ============================== (border of the popup window)
// [ ] US                         (|index| in the following functions is 0)
// [*] Anthy
// [ ] PinYin
// ------------------------------ (separator)
// [*] Hiragana                   (index = 5, The property has 2 radio groups)
// [ ] Katakana
// [ ] HalfWidthKatakana
// [*] Roman
// [ ] Kana
// ------------------------------ (separator)
// Configure IME...               (index = 11)
// ============================== (border of the popup window)
//
// Example of the menu (Simplified Chinese):
//
// ============================== (border of the popup window)
// [ ] US
// [ ] Anthy
// [*] PinYin
// ------------------------------ (separator)
// Switch to full letter mode     (The property has 2 command buttons)
// Switch to half punctuation mode
// ------------------------------ (separator)
// Configure IME...
// ============================== (border of the popup window)
//

namespace {

// Constants to specify the type of items in |model_|.
enum {
  COMMAND_ID_LANGUAGES = 0,  // US, Anthy, PinYin, ...
  COMMAND_ID_IME_PROPERTIES,  // Hiragana, Katakana, ...
  COMMAND_ID_CONFIGURE_IME,  // The "Configure IME..." button.
};

// A group ID for IME properties starts from 0. We use the huge value for the
// XKB/IME language list to avoid conflict.
const int kRadioGroupLanguage = 1 << 16;
const int kRadioGroupNone = -1;
const size_t kMaxLanguageNameLen = 7;
const wchar_t kSpacer[] = L"MMMMMMM";

// Converts chromeos::InputLanguage object into human readable string. Returns
// a string for the drop-down menu if |for_menu| is true. Otherwise, returns a
// string for the status area.
std::string FormatInputLanguage(
    const chromeos::InputLanguage& language, bool for_menu) {
  std::string formatted = language.display_name;
  if (formatted.empty()) {
    formatted = language.id;
  }
  if (for_menu) {
    switch (language.category) {
      case chromeos::LANGUAGE_CATEGORY_XKB:
        // TODO(yusukes): Use message catalog.
        formatted += " (Layout)";
        break;
      case chromeos::LANGUAGE_CATEGORY_IME:
        // TODO(yusukes): Use message catalog.
        formatted += " (IME)";
        break;
    }
  } else {
    // For status area. Trim the string.
    formatted = formatted.substr(0, kMaxLanguageNameLen);
    // TODO(yusukes): Simple substr() does not work for non-ASCII string.
    // TODO(yusukes): How can we ensure that the trimmed string does not
    // overflow the area?
  }
  return formatted;
}

}  // namespace

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// LanguageMenuButton

LanguageMenuButton::LanguageMenuButton(StatusAreaHost* host)
    : MenuButton(NULL, std::wstring(), this, false),
      language_list_(LanguageLibrary::Get()->GetActiveLanguages()),
      model_(NULL),
      // Be aware that the constructor of |language_menu_| calls GetItemCount()
      // in this class. Therefore, GetItemCount() have to return 0 when
      // |model_| is NULL.
      ALLOW_THIS_IN_INITIALIZER_LIST(language_menu_(this)),
      host_(host) {
  DCHECK(language_list_.get() && !language_list_->empty());
  // Update the model
  RebuildModel();
  // Grab the real estate.
  UpdateIcon(kSpacer);
  // Display the default XKB name (usually "US").
  const std::string name = FormatInputLanguage(language_list_->at(0), false);
  UpdateIcon(UTF8ToWide(name));
  LanguageLibrary::Get()->AddObserver(this);
}

LanguageMenuButton::~LanguageMenuButton() {
  LanguageLibrary::Get()->RemoveObserver(this);
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
  DCHECK(language_list_.get());

  if (IndexIsInLanguageList(index)) {
    const InputLanguage& language = language_list_->at(index);
    return language == LanguageLibrary::Get()->current_language();
  }

  if (GetPropertyIndex(index, &index)) {
    const ImePropertyList& property_list
        = LanguageLibrary::Get()->current_ime_properties();
    return property_list.at(index).is_selection_item_checked;
  }

  // Separator(s) or the "Configure IME" button.
  return false;
}

int LanguageMenuButton::GetGroupIdAt(int index) const {
  DCHECK_GE(index, 0);

  if (IndexIsInLanguageList(index)) {
    return kRadioGroupLanguage;
  }

  if (GetPropertyIndex(index, &index)) {
    const ImePropertyList& property_list
        = LanguageLibrary::Get()->current_ime_properties();
    return property_list.at(index).selection_item_id;
  }

  return kRadioGroupNone;
}

bool LanguageMenuButton::HasIcons() const  {
  // TODO(yusukes): Display IME icons.
  return false;
}

bool LanguageMenuButton::GetIconAt(int index, SkBitmap* icon) const {
  // TODO(yusukes): Display IME icons.
  return false;
}

bool LanguageMenuButton::IsEnabledAt(int index) const {
  // Just return true so all IMEs, XKB layouts, and IME properties could be
  // clicked.
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
    return menus::MenuModel::TYPE_COMMAND;  // "Configure IME"
  }

  if (IndexIsInLanguageList(index)) {
    return menus::MenuModel::TYPE_RADIO;
  }

  if (GetPropertyIndex(index, &index)) {
    const ImePropertyList& property_list
        = LanguageLibrary::Get()->current_ime_properties();
    if (property_list.at(index).is_selection_item) {
      return menus::MenuModel::TYPE_RADIO;
    }
    return menus::MenuModel::TYPE_COMMAND;
  }

  return menus::MenuModel::TYPE_SEPARATOR;
}

string16 LanguageMenuButton::GetLabelAt(int index) const {
  DCHECK_GE(index, 0);
  DCHECK(language_list_.get());

  if (IndexPointsToConfigureImeMenuItem(index)) {
    // TODO(yusukes): Use message catalog.
    return WideToUTF16(L"Configure IME...");
  }

  std::string name;
  if (IndexIsInLanguageList(index)) {
    name = FormatInputLanguage(language_list_->at(index), true);
  } else if (GetPropertyIndex(index, &index)) {
    const ImePropertyList& property_list
        = LanguageLibrary::Get()->current_ime_properties();
    name = property_list.at(index).label;
  }

  return UTF8ToUTF16(name);
}

void LanguageMenuButton::ActivatedAt(int index) {
  DCHECK_GE(index, 0);
  DCHECK(language_list_.get());

  if (IndexPointsToConfigureImeMenuItem(index)) {
    host_->OpenSystemOptionsDialog();
    return;
  }

  if (IndexIsInLanguageList(index)) {
    // Inter-IME switching or IME-XKB switching.
    const InputLanguage& language = language_list_->at(index);
    LanguageLibrary::Get()->ChangeLanguage(language.category, language.id);
    return;
  }

  if (GetPropertyIndex(index, &index)) {
    // Intra-IME switching (e.g. Japanese-Hiragana to Japanese-Katakana).
    const ImePropertyList& property_list
        = LanguageLibrary::Get()->current_ime_properties();
    const std::string key = property_list.at(index).key;
    if (property_list.at(index).is_selection_item) {
      // Radio button is clicked.
      const int id = property_list.at(index).selection_item_id;
      // First, deactivate all other properties in the same radio group.
      for (int i = 0; i < static_cast<int>(property_list.size()); ++i) {
        if (i != index && id == property_list.at(i).selection_item_id) {
          LanguageLibrary::Get()->DeactivateImeProperty(
              property_list.at(i).key);
        }
      }
      // Then, activate the property clicked.
      LanguageLibrary::Get()->ActivateImeProperty(key);
    } else {
      // Command button like "Switch to half punctuation mode" is clicked.
      // We can always use "Deactivate" for command buttons.
      LanguageLibrary::Get()->DeactivateImeProperty(key);
    }
    return;
  }

  // Separators are not clickable.
  NOTREACHED();
}

////////////////////////////////////////////////////////////////////////////////
// LanguageMenuButton, views::ViewMenuDelegate implementation:

void LanguageMenuButton::RunMenu(views::View* source, const gfx::Point& pt) {
  language_list_.reset(LanguageLibrary::Get()->GetActiveLanguages());
  RebuildModel();
  language_menu_.Rebuild();
  language_menu_.UpdateStates();
  language_menu_.RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
}

////////////////////////////////////////////////////////////////////////////////
// LanguageLibrary::Observer implementation:

void LanguageMenuButton::LanguageChanged(LanguageLibrary* obj) {
  const std::string name = FormatInputLanguage(obj->current_language(), false);
  UpdateIcon(UTF8ToWide(name));
}

void LanguageMenuButton::ImePropertiesChanged(LanguageLibrary* obj) {
  RebuildModel();
}

void LanguageMenuButton::UpdateIcon(const std::wstring& name) {
  set_border(NULL);
  SetFont(ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::BaseFont).DeriveFont(0, gfx::Font::BOLD));
  SetEnabledColor(SK_ColorWHITE);
  SetShowHighlighted(false);
  SetText(name);
  // TODO(yusukes): Show icon on the status area?
  set_alignment(TextButton::ALIGN_RIGHT);
  SchedulePaint();
}

void LanguageMenuButton::RebuildModel() {
  model_.reset(new menus::SimpleMenuModel(NULL));
  string16 dummy_label = UTF8ToUTF16("");
  // Indicates if separator's needed before each section.
  bool need_separator = false;

  if (!language_list_->empty()) {
    // We "abuse" the command_id and group_id arguments of AddRadioItem method.
    // A COMMAND_ID_XXX enum value is passed as command_id, and array index of
    // |language_list_| or |property_list| is passed as group_id.
    for (size_t i = 0; i < language_list_->size(); ++i) {
      model_->AddRadioItem(COMMAND_ID_LANGUAGES, dummy_label, i);
    }
    need_separator = true;
  }

  const ImePropertyList& property_list
      = LanguageLibrary::Get()->current_ime_properties();
  if (!property_list.empty()) {
    if (need_separator)
      model_->AddSeparator();
    for (size_t i = 0; i < property_list.size(); ++i) {
      model_->AddRadioItem(COMMAND_ID_IME_PROPERTIES, dummy_label, i);
    }
    need_separator = true;
  }

  // Note: We use AddSeparator() for separators, and AddRadioItem() for all
  // other items even if an item is not actually a radio item.
  if (need_separator)
    model_->AddSeparator();
  model_->AddRadioItem(COMMAND_ID_CONFIGURE_IME, dummy_label, 0 /* dummy */);
}

bool LanguageMenuButton::IndexIsInLanguageList(int index) const {
  DCHECK_GE(index, 0);
  DCHECK(model_.get());

  return ((model_->GetTypeAt(index) == menus::MenuModel::TYPE_RADIO) &&
          (model_->GetCommandIdAt(index) == COMMAND_ID_LANGUAGES));
}

bool LanguageMenuButton::GetPropertyIndex(
    int index, int* property_index) const {
  DCHECK_GE(index, 0);
  DCHECK(property_index);
  DCHECK(model_.get());

  if ((model_->GetTypeAt(index) == menus::MenuModel::TYPE_RADIO) &&
      (model_->GetCommandIdAt(index) == COMMAND_ID_IME_PROPERTIES)) {
    *property_index = model_->GetGroupIdAt(index);
    return true;
  }
  return false;
}

bool LanguageMenuButton::IndexPointsToConfigureImeMenuItem(int index) const {
  DCHECK_GE(index, 0);
  DCHECK(model_.get());

  return ((model_->GetTypeAt(index) == menus::MenuModel::TYPE_RADIO) &&
          (model_->GetCommandIdAt(index) == COMMAND_ID_CONFIGURE_IME));
}

// TODO(yusukes): Register and handle hotkeys for IME and XKB switching?

}  // namespace chromeos
