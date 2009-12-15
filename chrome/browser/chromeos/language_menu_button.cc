// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/language_menu_button.h"

#include <string>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/time.h"
#include "chrome/browser/browser.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace {

const int kRadioGroupNone = 0;
const int kRadioGroupLanguage = 1;
const size_t kMaxLanguageNameLen = 7;
const wchar_t kSpacer[] = L"MMMMMMM";

// Returns true if the |index| points to the "Configure IME" menu item.
bool IsIndexShowControlPanel(
    int index, chromeos::InputLanguageList* language_list) {
  DCHECK_GE(index, 0);
  if (language_list->empty()) {
    // If language_list is empty, then there's no separator. So "Configure IME"
    // should be at index 0.
    DCHECK_EQ(index, 0);
    return index == 0;
  }
  return static_cast<size_t>(index) == (language_list->size() + 1);
}

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

LanguageMenuButton::LanguageMenuButton(Browser* browser)
    : MenuButton(NULL, std::wstring(), this, false),
      language_list_(LanguageLibrary::Get()->GetLanguages()),
      // Since the constructor of |language_menu_| calls this->GetItemCount(),
      // we have to initialize |language_list_| before hand.
      ALLOW_THIS_IN_INITIALIZER_LIST(language_menu_(this)),
      browser_(browser) {
  DCHECK(language_list_.get() && !language_list_->empty());
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
  return index;  // dummy
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
  if (static_cast<size_t>(index) < language_list_->size()) {
    const InputLanguage& language = language_list_->at(index);
    return language == LanguageLibrary::Get()->current_language();
  }
  return false;
}

int LanguageMenuButton::GetGroupIdAt(int index) const {
  DCHECK_GE(index, 0);
  DCHECK(language_list_.get());
  if (static_cast<size_t>(index) < language_list_->size()) {
    return kRadioGroupLanguage;
  }
  return kRadioGroupNone;
}

bool LanguageMenuButton::HasIcons() const  {
  // TODO(yusukes): Display IME icons.
  return false;
}

bool LanguageMenuButton::GetIconAt(int index, SkBitmap* icon) const {
  return false;
}

bool LanguageMenuButton::IsEnabledAt(int index) const {
  // Just return true so all IMEs and XLB layouts listed could be clicked.
  return true;
}

menus::MenuModel* LanguageMenuButton::GetSubmenuModelAt(int index) const {
  return NULL;
}

void LanguageMenuButton::HighlightChangedTo(int index) {
  // Views for Chromium OS does not support this interface yet.
}

void LanguageMenuButton::MenuWillShow() {
  // Views for Chromium OS does not support this interface yet.
}

int LanguageMenuButton::GetItemCount() const {
  DCHECK(language_list_.get());
  if (language_list_->empty()) {
    return 1;  // no separator; "Configure IME" only
  }
  return language_list_->size() + 2;  // separator + "Configure IME"
}

menus::MenuModel::ItemType LanguageMenuButton::GetTypeAt(int index) const {
  DCHECK_GE(index, 0);
  DCHECK(language_list_.get());
  if (IsIndexShowControlPanel(index, language_list_.get())) {
    return menus::MenuModel::TYPE_COMMAND;  // "Configure IME"
  }
  if (static_cast<size_t>(index) < language_list_->size()) {
    return menus::MenuModel::TYPE_RADIO;
  }

  DCHECK_EQ(static_cast<size_t>(index), language_list_->size());
  return menus::MenuModel::TYPE_SEPARATOR;
}

string16 LanguageMenuButton::GetLabelAt(int index) const {
  DCHECK_GE(index, 0);
  DCHECK(language_list_.get());
  if (IsIndexShowControlPanel(index, language_list_.get())) {
    // TODO(yusukes): Use message catalog.
    return WideToUTF16(L"Configure IME...");
  }
  if (static_cast<size_t>(index) < language_list_->size()) {
    std::string name = FormatInputLanguage(language_list_->at(index), true);
    return UTF8ToUTF16(name);
  }
  NOTREACHED();
  return WideToUTF16(L"");
}

void LanguageMenuButton::ActivatedAt(int index) {
  DCHECK_GE(index, 0);
  DCHECK(language_list_.get());
  if (IsIndexShowControlPanel(index, language_list_.get())) {
    browser_->ShowControlPanel();
    return;
  }
  if (static_cast<size_t>(index) < language_list_->size()) {
    const InputLanguage& language = language_list_->at(index);
    LanguageLibrary::Get()->ChangeLanguage(language.category, language.id);
    return;
  }
  NOTREACHED();
}

////////////////////////////////////////////////////////////////////////////////
// LanguageMenuButton, views::ViewMenuDelegate implementation:

void LanguageMenuButton::RunMenu(views::View* source, const gfx::Point& pt) {
  language_list_.reset(LanguageLibrary::Get()->GetLanguages());
  language_menu_.Rebuild();
  language_menu_.UpdateStates();
  language_menu_.RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
}

////////////////////////////////////////////////////////////////////////////////
// LanguageMenuButton, PowerLibrary::Observer implementation:

void LanguageMenuButton::LanguageChanged(LanguageLibrary* obj) {
  const std::string name = FormatInputLanguage(obj->current_language(), false);
  UpdateIcon(UTF8ToWide(name));
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

// TODO(yusukes): Register and handle hotkeys for IME and XKB switching?

}  // namespace chromeos

