// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/eula_view.h"

#include <signal.h>
#include <sys/types.h>
#include <string>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/network_screen_delegate.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/installer/util/google_update_settings.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/button/native_button.h"
#include "views/controls/label.h"
#include "views/controls/textfield/textfield.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"

namespace {

const int kBorderSize = 10;
const int kMargin = 20;
const int kLastButtonHorizontalMargin = 10;
const int kTextMargin = 10;
const int kCheckBowWidth = 22;

// Fake EULA texts. TODO(glotov): implement reading actual file.
const wchar_t kLoremIpsum[] = L"Lorem ipsum dolor sit amet, "
    L"consectetur adipisicing elit, sed do eiusmod tempor incididunt ut"
    L"labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud "
    L"exercitation ullamco laboris nisi ut aliquip ex ea commodo "
    L"consequat. Duis aute irure dolor in reprehenderit in voluptate velit "
    L"esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat "
    L"cupidatat non proident, sunt in culpa qui officia deserunt mollit anim "
    L"id est laborum.\n";
const wchar_t kFakeGoogleEula[] = L"\nGoogle Chrome Terms of Service\n"
    L"These Terms of Service apply to the executable code version of "
    L"Google Chrome. ";
const wchar_t kFakeOemEula[] = L"\nYBH Terms of Service\n";

enum kLayoutColumnsets {
  SINGLE_CONTROL_ROW,
  SINGLE_CONTROL_WITH_SHIFT_ROW,
  SINGLE_LINK_WITH_SHIFT_ROW,
  LAST_ROW
};

}  // namespace

namespace chromeos {

EulaView::EulaView(chromeos::ScreenObserver* observer)
    : google_eula_text_(NULL),
      usage_statistics_checkbox_(NULL),
      learn_more_link_(NULL),
      oem_eula_text_(NULL),
      system_security_settings_link_(NULL),
      cancel_button_(NULL),
      continue_button_(NULL),
      observer_(observer) {
}

EulaView::~EulaView() {
}

void EulaView::Init() {
  // Use rounded rect background.
  views::Painter* painter = CreateWizardPainter(
      &BorderDefinition::kScreenBorder);
  set_background(
      views::Background::CreateBackgroundPainter(true, painter));

  // Layout created controls.
  static const int kPadding = kBorderSize + kMargin;
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  views::ColumnSet* column_set = layout->AddColumnSet(SINGLE_CONTROL_ROW);
  column_set->AddPaddingColumn(0, kPadding);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kPadding);

  column_set = layout->AddColumnSet(SINGLE_CONTROL_WITH_SHIFT_ROW);
  column_set->AddPaddingColumn(0, kPadding + kTextMargin);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kPadding);

  column_set = layout->AddColumnSet(SINGLE_LINK_WITH_SHIFT_ROW);
  column_set->AddPaddingColumn(0, kPadding + kTextMargin + kCheckBowWidth);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kPadding);

  column_set = layout->AddColumnSet(LAST_ROW);
  column_set->AddPaddingColumn(0, kPadding + kTextMargin);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 0,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 0,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kLastButtonHorizontalMargin + kBorderSize);

  layout->AddPaddingRow(0, kPadding);
  layout->StartRow(1, SINGLE_CONTROL_ROW);
  google_eula_text_ = new views::Textfield(views::Textfield::STYLE_MULTILINE);
  google_eula_text_->SetReadOnly(true);
  google_eula_text_->SetFocusable(true);
  google_eula_text_->SetHorizontalMargins(kTextMargin, kTextMargin);
  layout->AddView(google_eula_text_);

  layout->StartRow(0, SINGLE_CONTROL_WITH_SHIFT_ROW);
  usage_statistics_checkbox_ = new views::Checkbox();
  usage_statistics_checkbox_->SetMultiLine(true);
  usage_statistics_checkbox_->SetChecked(
      GoogleUpdateSettings::GetCollectStatsConsent());
  layout->AddView(usage_statistics_checkbox_);

  layout->StartRow(0, SINGLE_LINK_WITH_SHIFT_ROW);
  learn_more_link_ = new views::Link();
  learn_more_link_->SetController(this);
  layout->AddView(learn_more_link_);

  layout->AddPaddingRow(0, kRelatedControlSmallVerticalSpacing);
  layout->StartRow(1, SINGLE_CONTROL_ROW);
  oem_eula_text_ = new views::Textfield(views::Textfield::STYLE_MULTILINE);
  oem_eula_text_->SetReadOnly(true);
  oem_eula_text_->SetFocusable(true);
  oem_eula_text_->SetHorizontalMargins(kTextMargin, kTextMargin);
  layout->AddView(oem_eula_text_);

  layout->AddPaddingRow(0, kRelatedControlSmallVerticalSpacing);
  layout->StartRow(0, LAST_ROW);
  system_security_settings_link_ = new views::Link();
  system_security_settings_link_->SetController(this);
  layout->AddView(system_security_settings_link_);

  cancel_button_ = new views::NativeButton(this, std::wstring());
  cancel_button_->SetEnabled(false);
  layout->AddView(cancel_button_);

  continue_button_ = new views::NativeButton(this, std::wstring());
  layout->AddView(continue_button_);
  layout->AddPaddingRow(0, kPadding);

  UpdateLocalizedStrings();
}

void EulaView::UpdateLocalizedStrings() {
  google_eula_text_->SetText(WideToUTF16(kFakeGoogleEula) +
                             WideToUTF16(kLoremIpsum) +
                             WideToUTF16(kLoremIpsum));
  oem_eula_text_->SetText(WideToUTF16(kFakeOemEula) +
                          WideToUTF16(kLoremIpsum) +
                          WideToUTF16(kLoremIpsum));
  usage_statistics_checkbox_->SetLabel(
      l10n_util::GetString(IDS_EULA_CHECKBOX_ENABLE_LOGGING));
  learn_more_link_->SetText(
      l10n_util::GetString(IDS_LEARN_MORE));
  system_security_settings_link_->SetText(
      l10n_util::GetString(IDS_EULA_SYSTEM_SECURITY_SETTINGS_LINK));
  continue_button_->SetLabel(
      l10n_util::GetString(IDS_EULA_ACCEPT_AND_CONTINUE_BUTTON));
  cancel_button_->SetLabel(
      l10n_util::GetString(IDS_CANCEL));
}

////////////////////////////////////////////////////////////////////////////////
// views::View: implementation:

void EulaView::LocaleChanged() {
  UpdateLocalizedStrings();
  Layout();
}

////////////////////////////////////////////////////////////////////////////////
// views::ButtonListener implementation:

void EulaView::ButtonPressed(views::Button* sender, const views::Event& event) {
  if (sender == continue_button_) {
    if (usage_statistics_checkbox_) {
      GoogleUpdateSettings::SetCollectStatsConsent(
        usage_statistics_checkbox_->checked());
    }
    observer_->OnExit(ScreenObserver::EULA_ACCEPTED);
  }
  // TODO(glotov): handle cancel button.
}

////////////////////////////////////////////////////////////////////////////////
// views::LinkController implementation:

void EulaView::LinkActivated(views::Link* source, int event_flags) {
  // TODO(glotov): handle link clicks.
}

}  // namespace chromeos
