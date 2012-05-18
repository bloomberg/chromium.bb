// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/wimax_config_view.h"

#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/cros/onc_constants.h"
#include "chrome/browser/chromeos/enrollment_dialog_view.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

WimaxConfigView::WimaxConfigView(NetworkConfigView* parent, WimaxNetwork* wimax)
    : ChildNetworkConfigView(parent, wimax),
      identity_label_(NULL),
      identity_textfield_(NULL),
      save_credentials_checkbox_(NULL),
      share_network_checkbox_(NULL),
      shared_network_label_(NULL),
      passphrase_label_(NULL),
      passphrase_textfield_(NULL),
      passphrase_visible_button_(NULL),
      error_label_(NULL) {
  Init(wimax);
}

WimaxConfigView::~WimaxConfigView() {
}

string16 WimaxConfigView::GetTitle() {
  // TODO(stevenjb): Replace this generic "Connect to network..." string with
  // WiMAX-specific one.
  return l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CONNECT_TITLE);
}

views::View* WimaxConfigView::GetInitiallyFocusedView() {
  if (identity_textfield_ && identity_textfield_->enabled())
    return identity_textfield_;
  if (passphrase_textfield_ && passphrase_textfield_->enabled())
    return passphrase_textfield_;
  return NULL;
}

bool WimaxConfigView::CanLogin() {
  // TODO(benchan): Update this with the correct minimum length.
  static const size_t kMinWimaxPasswordLen = 0;

  // If the network requires a passphrase, make sure it is the right length.
  return passphrase_textfield_ && passphrase_textfield_->text().length() >=
      kMinWimaxPasswordLen;
}

void WimaxConfigView::UpdateDialogButtons() {
  parent_->GetDialogClientView()->UpdateDialogButtons();
}

void WimaxConfigView::RefreshShareCheckbox() {
  if (!share_network_checkbox_)
    return;

  if (!UserManager::Get()->IsUserLoggedIn()) {
    // If not logged in, networks must be shared.
    share_network_checkbox_->SetEnabled(false);
    share_network_checkbox_->SetChecked(true);
  } else {
    share_network_checkbox_->SetEnabled(true);
    share_network_checkbox_->SetChecked(false);  // Default to unshared.
  }
}

void WimaxConfigView::UpdateErrorLabel() {
  std::string error_msg;
  if (!service_path_.empty()) {
    NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
    const WimaxNetwork* wimax = cros->FindWimaxNetworkByPath(service_path_);
    if (wimax && wimax->failed()) {
      bool passphrase_empty = wimax->eap_passphrase().empty();
      switch (wimax->error()) {
        case ERROR_BAD_PASSPHRASE:
          if (!passphrase_empty) {
            error_msg = l10n_util::GetStringUTF8(
                IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_BAD_PASSPHRASE);
          }
          break;
        case ERROR_BAD_WEPKEY:
          if (!passphrase_empty) {
            error_msg = l10n_util::GetStringUTF8(
                IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_BAD_WEPKEY);
          }
          break;
        default:
          error_msg = wimax->GetErrorString();
          break;
      }
    }
  }
  if (!error_msg.empty()) {
    error_label_->SetText(UTF8ToUTF16(error_msg));
    error_label_->SetVisible(true);
  } else {
    error_label_->SetVisible(false);
  }
}

void WimaxConfigView::ContentsChanged(views::Textfield* sender,
                                      const string16& new_contents) {
  UpdateDialogButtons();
}

bool WimaxConfigView::HandleKeyEvent(views::Textfield* sender,
                                     const views::KeyEvent& key_event) {
  if (sender == passphrase_textfield_ &&
      key_event.key_code() == ui::VKEY_RETURN) {
    parent_->GetDialogClientView()->AcceptWindow();
  }
  return false;
}

void WimaxConfigView::ButtonPressed(views::Button* sender,
                                   const views::Event& event) {
  if (sender == passphrase_visible_button_) {
    if (passphrase_textfield_) {
      passphrase_textfield_->SetObscured(!passphrase_textfield_->IsObscured());
      passphrase_visible_button_->SetToggled(
          !passphrase_textfield_->IsObscured());
    }
  } else {
    NOTREACHED();
  }
}

bool WimaxConfigView::Login() {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  WimaxNetwork* wimax = cros->FindWimaxNetworkByPath(service_path_);
  if (!wimax) {
    // Flimflam no longer knows about this wimax network (edge case).
    // TODO(stevenjb): Add a notification (chromium-os13225).
    LOG(WARNING) << "Wimax network: " << service_path_ << " no longer exists.";
    return true;
  }
  wimax->SetEAPIdentity(GetEapIdentity());
  wimax->SetEAPPassphrase(GetEapPassphrase());
  bool share_default = (wimax->profile_type() != PROFILE_USER);
  bool share = GetShareNetwork(share_default);
  wimax->SetEnrollmentDelegate(
      CreateEnrollmentDelegate(GetWidget()->GetNativeWindow(),
                               wimax->name(),
                               ProfileManager::GetLastUsedProfile()));
  cros->ConnectToWimaxNetwork(wimax, share);
  // Connection failures are responsible for updating the UI, including
  // reopening dialogs.
  return true;  // dialog will be closed
}

std::string WimaxConfigView::GetEapIdentity() const {
  DCHECK(identity_textfield_);
  return UTF16ToUTF8(identity_textfield_->text());
}

std::string WimaxConfigView::GetEapPassphrase() const {
  return passphrase_textfield_ ? UTF16ToUTF8(passphrase_textfield_->text()) :
                                 std::string();
}

bool WimaxConfigView::GetSaveCredentials() const {
  return save_credentials_checkbox_ ? save_credentials_checkbox_->checked() :
                                      true;
}

bool WimaxConfigView::GetShareNetwork(bool share_default) const {
  return share_network_checkbox_ ? share_network_checkbox_->checked() :
                                   share_default;
}

void WimaxConfigView::Cancel() {
}

void WimaxConfigView::Init(WimaxNetwork* wimax) {
  DCHECK(wimax);
  WifiConfigView::ParseWiFiEAPUIProperty(
      &save_credentials_ui_data_, wimax, onc::eap::kSaveCredentials);
  WifiConfigView::ParseWiFiEAPUIProperty(
      &identity_ui_data_, wimax, onc::eap::kIdentity);
  WifiConfigView::ParseWiFiUIProperty(
      &passphrase_ui_data_, wimax, onc::wifi::kPassphrase);

  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  int column_view_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(column_view_set_id);
  const int kPasswordVisibleWidth = 20;
  // Label
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, views::kRelatedControlSmallHorizontalSpacing);
  // Textfield, combobox.
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0,
                        ChildNetworkConfigView::kInputFieldMinWidth);
  column_set->AddPaddingColumn(0, views::kRelatedControlSmallHorizontalSpacing);
  // Password visible button / policy indicator.
  column_set->AddColumn(views::GridLayout::CENTER, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, kPasswordVisibleWidth);

  // Netowrk name
  layout->StartRow(0, column_view_set_id);
  layout->AddView(new views::Label(l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_INTERNET_TAB_NETWORK)));
  views::Label* label = new views::Label(UTF8ToUTF16(wimax->name()));
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  layout->AddView(label);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Identity
  layout->StartRow(0, column_view_set_id);
  identity_label_ = new views::Label(l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_IDENTITY));
  layout->AddView(identity_label_);
  identity_textfield_ = new views::Textfield(
      views::Textfield::STYLE_DEFAULT);
  identity_textfield_->SetController(this);
  const std::string& eap_identity = wimax->eap_identity();
  identity_textfield_->SetText(UTF8ToUTF16(eap_identity));
  identity_textfield_->SetEnabled(identity_ui_data_.editable());
  layout->AddView(identity_textfield_);
  layout->AddView(new ControlledSettingIndicatorView(identity_ui_data_));
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Passphrase input
  layout->StartRow(0, column_view_set_id);
  int label_text_id = IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PASSPHRASE;
  passphrase_label_ = new views::Label(
      l10n_util::GetStringUTF16(label_text_id));
  layout->AddView(passphrase_label_);
  passphrase_textfield_ = new views::Textfield(
      views::Textfield::STYLE_OBSCURED);
  passphrase_textfield_->SetController(this);
  passphrase_label_->SetEnabled(true);
  passphrase_textfield_->SetEnabled(passphrase_ui_data_.editable());
  passphrase_textfield_->SetAccessibleName(l10n_util::GetStringUTF16(
      label_text_id));
  layout->AddView(passphrase_textfield_);

  if (passphrase_ui_data_.managed()) {
    layout->AddView(new ControlledSettingIndicatorView(passphrase_ui_data_));
  } else {
    // Password visible button.
    passphrase_visible_button_ = new views::ToggleImageButton(this);
    passphrase_visible_button_->SetTooltipText(
        l10n_util::GetStringUTF16(
            IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PASSPHRASE_SHOW));
    passphrase_visible_button_->SetToggledTooltipText(
        l10n_util::GetStringUTF16(
            IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PASSPHRASE_HIDE));
    passphrase_visible_button_->SetImage(
        views::ImageButton::BS_NORMAL,
        ResourceBundle::GetSharedInstance().
        GetBitmapNamed(IDR_NETWORK_SHOW_PASSWORD_OFF));
    passphrase_visible_button_->SetImage(
        views::ImageButton::BS_HOT,
        ResourceBundle::GetSharedInstance().
        GetBitmapNamed(IDR_NETWORK_SHOW_PASSWORD_HOVER));
    passphrase_visible_button_->SetToggledImage(
        views::ImageButton::BS_NORMAL,
        ResourceBundle::GetSharedInstance().
        GetBitmapNamed(IDR_NETWORK_SHOW_PASSWORD_ON));
    passphrase_visible_button_->SetImageAlignment(
        views::ImageButton::ALIGN_CENTER, views::ImageButton::ALIGN_MIDDLE);
    layout->AddView(passphrase_visible_button_);
  }

  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Checkboxes.

  // Save credentials
  layout->StartRow(0, column_view_set_id);
  save_credentials_checkbox_ = new views::Checkbox(
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SAVE_CREDENTIALS));
  save_credentials_checkbox_->SetEnabled(
      save_credentials_ui_data_.editable());
  layout->SkipColumns(1);
  layout->AddView(save_credentials_checkbox_);
  layout->AddView(
      new ControlledSettingIndicatorView(save_credentials_ui_data_));

  // Share network
  if (wimax->profile_type() == PROFILE_NONE && wimax->passphrase_required()) {
    layout->StartRow(0, column_view_set_id);
    share_network_checkbox_ = new views::Checkbox(
        l10n_util::GetStringUTF16(
            IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SHARE_NETWORK));
    layout->SkipColumns(1);
    layout->AddView(share_network_checkbox_);
  }
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Create an error label.
  layout->StartRow(0, column_view_set_id);
  layout->SkipColumns(1);
  error_label_ = new views::Label();
  error_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  error_label_->SetEnabledColor(SK_ColorRED);
  layout->AddView(error_label_);

  RefreshShareCheckbox();
  UpdateErrorLabel();
}

void WimaxConfigView::InitFocus() {
  views::View* view_to_focus = GetInitiallyFocusedView();
  if (view_to_focus)
    view_to_focus->RequestFocus();
}

}  // namespace chromeos
