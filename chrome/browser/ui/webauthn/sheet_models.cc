// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/sheet_models.h"

#include <vector>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

// AuthenticatorSheetModelBase ------------------------------------------------

AuthenticatorSheetModelBase::AuthenticatorSheetModelBase(
    AuthenticatorRequestDialogModel* dialog_model)
    : dialog_model_(dialog_model) {
  DCHECK(dialog_model);
  dialog_model_->AddObserver(this);
}

AuthenticatorSheetModelBase::~AuthenticatorSheetModelBase() {
  if (dialog_model_) {
    dialog_model_->RemoveObserver(this);
    dialog_model_ = nullptr;
  }
}

// static
gfx::ImageSkia* AuthenticatorSheetModelBase::GetImage(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id);
}

bool AuthenticatorSheetModelBase::IsBackButtonVisible() const {
  return true;
}

bool AuthenticatorSheetModelBase::IsCancelButtonVisible() const {
  return true;
}

base::string16 AuthenticatorSheetModelBase::GetCancelButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_CANCEL);
}

bool AuthenticatorSheetModelBase::IsAcceptButtonVisible() const {
  return false;
}

bool AuthenticatorSheetModelBase::IsAcceptButtonEnabled() const {
  NOTREACHED();
  return false;
}

base::string16 AuthenticatorSheetModelBase::GetAcceptButtonLabel() const {
  NOTREACHED();
  return base::string16();
}

void AuthenticatorSheetModelBase::OnBack() {
  if (dialog_model())
    dialog_model()->Back();
}

void AuthenticatorSheetModelBase::OnAccept() {
  NOTREACHED();
}

void AuthenticatorSheetModelBase::OnCancel() {
  if (dialog_model())
    dialog_model()->Cancel();
}

void AuthenticatorSheetModelBase::OnModelDestroyed() {
  dialog_model_ = nullptr;
}

// AuthenticatorInitialSheetModel ---------------------------------------------

gfx::ImageSkia* AuthenticatorInitialSheetModel::GetStepIllustration() const {
  return GetImage(IDR_WEBAUTHN_ILLUSTRATION_WELCOME_1X);
}

base::string16 AuthenticatorInitialSheetModel::GetStepTitle() const {
  // TODO(hongjunchoi): Insert actual domain name from model to
  // |application_name|.
  base::string16 application_name = base::UTF8ToUTF16("example.com");
  return l10n_util::GetStringFUTF16(IDS_WEBAUTHN_INITIAL_SHEET_TITLE,
                                    application_name);
}

base::string16 AuthenticatorInitialSheetModel::GetStepDescription() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_INITIAL_SHEET_DESCRIPTION);
}

bool AuthenticatorInitialSheetModel::IsAcceptButtonVisible() const {
  return true;
}

bool AuthenticatorInitialSheetModel::IsAcceptButtonEnabled() const {
  return true;
}

base::string16 AuthenticatorInitialSheetModel::GetAcceptButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_INITIAL_SHEET_NEXT);
}

void AuthenticatorInitialSheetModel::OnAccept() {
  // TODO(hongjunchoi): Check whether Bluetooth adapter is enabled and if it is,
  // set current step to |kTransportSelection|.
  dialog_model()->SetCurrentStep(
      AuthenticatorRequestDialogModel::Step::kUsbInsertAndActivateOnRegister);
}

// AuthenticatorTransportSelectorSheetModel -----------------------------------

gfx::ImageSkia* AuthenticatorTransportSelectorSheetModel::GetStepIllustration()
    const {
  return GetImage(IDR_WEBAUTHN_ILLUSTRATION_WELCOME_1X);
}

base::string16 AuthenticatorTransportSelectorSheetModel::GetStepTitle() const {
  // TODO(hongjunchoi): Insert actual domain name from model to
  // |application_name|.
  base::string16 application_name = base::UTF8ToUTF16("example.com");
  return l10n_util::GetStringFUTF16(IDS_WEBAUTHN_TRANSPORT_SELECTION_TITLE,
                                    application_name);
}

base::string16 AuthenticatorTransportSelectorSheetModel::GetStepDescription()
    const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_TRANSPORT_SELECTION_DESCRIPTION);
}

void AuthenticatorTransportSelectorSheetModel::OnTransportSelected(
    AuthenticatorTransport transport) {
  dialog_model()->StartGuidedFlowForTransport(transport);
}

// AuthenticatorInsertAndActivateUsbOnRegisterSheetModel ----------------------

gfx::ImageSkia*
AuthenticatorInsertAndActivateUsbOnRegisterSheetModel::GetStepIllustration()
    const {
  return GetImage(IDR_WEBAUTHN_ILLUSTRATION_USB_1X);
}

base::string16
AuthenticatorInsertAndActivateUsbOnRegisterSheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_USB_TITLE_ON_REGISTER);
}

base::string16
AuthenticatorInsertAndActivateUsbOnRegisterSheetModel::GetStepDescription()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_USB_INSERT_DESCRIPTION);
}

// AuthenticatorInsertAndActivateUsbOnSignSheetModel ----------------------

gfx::ImageSkia*
AuthenticatorInsertAndActivateUsbOnSignSheetModel::GetStepIllustration() const {
  return GetImage(IDR_WEBAUTHN_ILLUSTRATION_USB_1X);
}

base::string16 AuthenticatorInsertAndActivateUsbOnSignSheetModel::GetStepTitle()
    const {
  // TODO(hongjunchoi): Insert actual domain name from model to
  // |application_name|.
  base::string16 application_name = base::UTF8ToUTF16("example.com");
  return l10n_util::GetStringFUTF16(IDS_WEBAUTHN_USB_TITLE_ON_SIGNIN,
                                    application_name);
}

base::string16
AuthenticatorInsertAndActivateUsbOnSignSheetModel::GetStepDescription() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_USB_INSERT_DESCRIPTION);
}

// AuthenticatorTimeoutErrorModel ---------------------------------------------

gfx::ImageSkia* AuthenticatorTimeoutErrorModel::GetStepIllustration() const {
  return GetImage(IDR_WEBAUTHN_ILLUSTRATION_ERROR_TIMEOUT_1X);
}

base::string16 AuthenticatorTimeoutErrorModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_TIMEOUT_TITLE);
}

base::string16 AuthenticatorTimeoutErrorModel::GetStepDescription() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_TIMEOUT_DESCRIPTION);
}

// AuthenticatorBlePowerOnManualSheetModel ------------------------------------

gfx::ImageSkia* AuthenticatorBlePowerOnManualSheetModel::GetStepIllustration()
    const {
  return GetImage(IDR_WEBAUTHN_ILLUSTRATION_ERROR_BLUETOOTH_1X);
}

base::string16 AuthenticatorBlePowerOnManualSheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_BLE_POWER_ON_MANUAL_TITLE);
}

base::string16 AuthenticatorBlePowerOnManualSheetModel::GetStepDescription()
    const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_BLE_POWER_ON_MANUAL_DESCRIPTION);
}

bool AuthenticatorBlePowerOnManualSheetModel::IsAcceptButtonVisible() const {
  return true;
}

bool AuthenticatorBlePowerOnManualSheetModel::IsAcceptButtonEnabled() const {
  return true;
}

base::string16 AuthenticatorBlePowerOnManualSheetModel::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_BLE_POWER_ON_MANUAL_TRY_AGAIN);
}

// AuthenticatorBlePairingBeginSheetModel -------------------------------------

gfx::ImageSkia* AuthenticatorBlePairingBeginSheetModel::GetStepIllustration()
    const {
  return GetImage(IDR_WEBAUTHN_ILLUSTRATION_BLE_1X);
}

base::string16 AuthenticatorBlePairingBeginSheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_BLE_PAIRING_BEGIN_TITLE);
}

base::string16 AuthenticatorBlePairingBeginSheetModel::GetStepDescription()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_BLE_PAIRING_BEGIN_DESCRIPTION);
}

bool AuthenticatorBlePairingBeginSheetModel::IsAcceptButtonVisible() const {
  return true;
}

bool AuthenticatorBlePairingBeginSheetModel::IsAcceptButtonEnabled() const {
  return true;
}

base::string16 AuthenticatorBlePairingBeginSheetModel::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_BLE_PAIRING_BEGIN_START);
}

// AuthenticatorBleEnterPairingModeSheetModel ---------------------------------

gfx::ImageSkia*
AuthenticatorBleEnterPairingModeSheetModel::GetStepIllustration() const {
  return GetImage(IDR_WEBAUTHN_ILLUSTRATION_BLE_1X);
}

base::string16 AuthenticatorBleEnterPairingModeSheetModel::GetStepTitle()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_BLE_ENTER_PAIRING_MODE_TITLE);
}

base::string16 AuthenticatorBleEnterPairingModeSheetModel::GetStepDescription()
    const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_BLE_ENTER_PAIRING_MODE_DESCRIPTION);
}

// AuthenticatorBleDeviceSelectionSheetModel ----------------------------------

gfx::ImageSkia* AuthenticatorBleDeviceSelectionSheetModel::GetStepIllustration()
    const {
  return GetImage(IDR_WEBAUTHN_ILLUSTRATION_BLE_NAME_1X);
}

base::string16 AuthenticatorBleDeviceSelectionSheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_BLE_DEVICE_SELECTION_TITLE);
}

base::string16 AuthenticatorBleDeviceSelectionSheetModel::GetStepDescription()
    const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_BLE_DEVICE_SELECTION_DESCRIPTION);
}

// AuthenticatorBlePinEntrySheetModel -----------------------------------------

gfx::ImageSkia* AuthenticatorBlePinEntrySheetModel::GetStepIllustration()
    const {
  return GetImage(IDR_WEBAUTHN_ILLUSTRATION_BLE_PIN_1X);
}

base::string16 AuthenticatorBlePinEntrySheetModel::GetStepTitle() const {
  // TODO(hongjunchoi): Insert actual device name from model to |device_name|.
  base::string16 device_name = base::UTF8ToUTF16("VHGSHSSN");
  return l10n_util::GetStringFUTF16(IDS_WEBAUTHN_BLE_PIN_ENTRY_TITLE,
                                    device_name);
}

base::string16 AuthenticatorBlePinEntrySheetModel::GetStepDescription() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_BLE_PIN_ENTRY_DESCRIPTION);
}

bool AuthenticatorBlePinEntrySheetModel::IsAcceptButtonVisible() const {
  return true;
}

bool AuthenticatorBlePinEntrySheetModel::IsAcceptButtonEnabled() const {
  return true;
}

base::string16 AuthenticatorBlePinEntrySheetModel::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_BLE_PIN_ENTRY_NEXT);
}

// AuthenticatorBleVerifyingSheetModel ----------------------------------------

gfx::ImageSkia* AuthenticatorBleVerifyingSheetModel::GetStepIllustration()
    const {
  return GetImage(IDR_WEBAUTHN_ILLUSTRATION_BLE_1X);
}

base::string16 AuthenticatorBleVerifyingSheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_BLE_VERIFYING_TITLE);
}

base::string16 AuthenticatorBleVerifyingSheetModel::GetStepDescription() const {
  return base::string16();
}

// AuthenticatorBleActivateSheetModel -----------------------------------------

gfx::ImageSkia* AuthenticatorBleActivateSheetModel::GetStepIllustration()
    const {
  return GetImage(IDR_WEBAUTHN_ILLUSTRATION_BLE_TAP_1X);
}

base::string16 AuthenticatorBleActivateSheetModel::GetStepTitle() const {
  // TODO(hongjunchoi): Insert actual domain name from model to
  // |application_name|.
  base::string16 application_name = base::UTF8ToUTF16("example.com");
  return l10n_util::GetStringFUTF16(IDS_WEBAUTHN_BLE_ACTIVATE_TITLE,
                                    application_name);
}

base::string16 AuthenticatorBleActivateSheetModel::GetStepDescription() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_BLE_ACTIVATE_DESCRIPTION);
}
