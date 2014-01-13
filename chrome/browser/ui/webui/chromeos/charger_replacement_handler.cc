// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/charger_replacement_handler.h"

#include "base/bind.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/charger_replace/charger_link_dialog.h"
#include "chrome/browser/chromeos/charger_replace/charger_replacement_dialog.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

const char kFaqLink[] = "http://chromebook.com/hp11charger";

} // namespace

ChargerReplacementHandler::ChargerReplacementHandler(
    ChargerReplacementDialog* dialog)
    : charger_window_(NULL),
      dialog_(dialog) {
}

ChargerReplacementHandler::~ChargerReplacementHandler() {
}

void ChargerReplacementHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("confirmSafeCharger",
      base::Bind(&ChargerReplacementHandler::ConfirmSafeCharger,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("confirmNotOrderNewCharger",
      base::Bind(&ChargerReplacementHandler::ConfirmNotOrderNewCharger,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("confirmChargerOrderedOnline",
      base::Bind(&ChargerReplacementHandler::ConfirmChargerOrderedOnline,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("confirmChargerOrderByPhone",
      base::Bind(&ChargerReplacementHandler::ConfirmChargerOrderByPhone,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("confirmStillUseBadCharger",
      base::Bind(&ChargerReplacementHandler::ConfirmStillUseBadCharger,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("showLink",
      base::Bind(&ChargerReplacementHandler::ShowLink,
                 base::Unretained(this)));
}

// static
void ChargerReplacementHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  localized_strings->SetString(
      "checkChargerTitle",
      l10n_util::GetStringUTF16(IDS_CHARGER_REPLACEMENT_CHECK_CHARGER_TITLE));
  localized_strings->SetString(
      "checkChargerDamage",
      l10n_util::GetStringUTF16(IDS_CHARGER_REPLACEMENT_CHECK_CHARGER_DAMAGE));
  localized_strings->SetString(
      "checkOriginalCharger",
      l10n_util::GetStringUTF16(IDS_CHARGER_REPLACEMENT_CHECK_ORIGNAL_CHARGER));
  localized_strings->SetString(
      "whereDevicePurchased",
      l10n_util::GetStringUTF16(
          IDS_CHARGER_REPLACEMENT_WHERE_DEVICE_PURCHASED));
  localized_strings->SetString(
      "selectCountry",
      l10n_util::GetStringUTF16(
          IDS_CHARGER_REPLACEMENT_SELECT_A_COUNTRY));
  localized_strings->SetString(
      "us",
      l10n_util::GetStringUTF16(IDS_CHARGER_REPLACEMENT_US));
  localized_strings->SetString(
      "ca",
      l10n_util::GetStringUTF16(IDS_CHARGER_REPLACEMENT_CA));
  localized_strings->SetString(
      "uk",
      l10n_util::GetStringUTF16(IDS_CHARGER_REPLACEMENT_UK));
  localized_strings->SetString(
      "au",
      l10n_util::GetStringUTF16(IDS_CHARGER_REPLACEMENT_AU));
  localized_strings->SetString(
      "ire",
      l10n_util::GetStringUTF16(IDS_CHARGER_REPLACEMENT_IRE));
  localized_strings->SetString(
      "checkChargerSelectCharger",
      l10n_util::GetStringUTF16(
          IDS_CHARGER_REPLACEMENT_CHECK_CHARGER_SELECT_CHARGER));
  localized_strings->SetString(
      "nextStepButtonText",
      l10n_util::GetStringUTF16(IDS_CHARGER_REPLACEMENT_NEXT_STEP));
  localized_strings->SetString(
      "confirmSafeChargerTitle",
      l10n_util::GetStringUTF16(
          IDS_CHARGER_REPLACEMENT_CONFIRM_SAFE_CHARGER_TITLE));
  localized_strings->SetString(
      "goWithSafeCharger",
      l10n_util::GetStringUTF16(
          IDS_CHARGER_REPLACEMENT_CONFIRM_SAFE_CHARGER_TO_GO));
  localized_strings->SetString(
      "prevStepText",
      l10n_util::GetStringUTF16(IDS_CHARGER_REPLACEMENT_PREVIOUS_STEP));
  localized_strings->SetString(
      "finishText",
      l10n_util::GetStringUTF16(IDS_CHARGER_REPLACEMENT_FINISH));
  localized_strings->SetString(
      "chargerUpdateTitle",
      l10n_util::GetStringUTF16(
          IDS_CHARGER_REPLACEMENT_CHARGER_UPDATE_TITLE));
  localized_strings->SetString(
      "chargerUpdateP1",
      l10n_util::GetStringUTF16(
          IDS_CHARGER_REPLACEMENT_CHARGER_UPDATE_P1));
  localized_strings->SetString(
      "stopUsingRecalledCharger",
      l10n_util::GetStringUTF16(
          IDS_CHARGER_REPLACEMENT_CHARGER_STOP_USING_RECALLED_CHARGER));
  localized_strings->SetString(
      "chargerUpdateP2",
      l10n_util::GetStringUTF16(
          IDS_CHARGER_REPLACEMENT_CHARGER_UPDATE_P2));
  localized_strings->SetString(
      "chargerUpdateFAQ",
      l10n_util::GetStringUTF16(
          IDS_CHARGER_REPLACEMENT_CHARGER_UPDATE_FAQ));
  localized_strings->SetString(
      "orderNewCharger",
      l10n_util::GetStringUTF16(
          IDS_CHARGER_REPLACEMENT_CHARGER_ORDER_NEW_CHARGER));
  localized_strings->SetString(
      "notOrderNewCharger",
      l10n_util::GetStringUTF16(
          IDS_CHARGER_REPLACEMENT_CHARGER_NOT_ORDER_NEW_CHARGER));
  localized_strings->SetString(
      "confirmNotOrderNewCharger",
      l10n_util::GetStringUTF16(
          IDS_CHARGER_REPLACEMENT_CHARGER_CONIFRM_NOT_ORDER_CHARGER));
  localized_strings->SetString(
      "noMoreShowText",
      l10n_util::GetStringUTF16(
          IDS_CHARGER_REPLACEMENT_NO_MOPE_MESSAGE_SHOW));
  localized_strings->SetString(
      "finishNotOrderChargerTitle",
      l10n_util::GetStringUTF16(
          IDS_CHARGER_REPLACEMENT_FINISH_NOT_ORDER_CHARGER_TITLE));
  localized_strings->SetString(
      "finishNotOrderChargerP2",
      l10n_util::GetStringUTF16(
          IDS_CHARGER_REPLACEMENT_FINISH_NOT_ORDER_CHARGER_P2));
  localized_strings->SetString(
      "finishNotOrderChargerMoreInfo",
      l10n_util::GetStringUTF16(
          IDS_CHARGER_REPLACEMENT_FINISH_NOT_ORDER_CHARGER_MORE_INFO));

  localized_strings->SetString(
      "confirmOnlineOrder",
      l10n_util::GetStringUTF16(
          IDS_CHARGER_REPLACEMENT_ONLINE_ORDER_CONFIRMATION_TITLE));
  localized_strings->SetString(
      "confirmReceivingOnlineOrder",
      l10n_util::GetStringUTF16(
          IDS_CHARGER_REPLACEMENT_ONLINE_ORDER_CONFIRMATION_MESSAGE));
  localized_strings->SetString(
      "needMoreInformation",
      l10n_util::GetStringUTF16(IDS_CHARGER_REPLACEMENT_FIND_MORE_INFORMATION));
  localized_strings->SetString(
      "orderChargerOfflineTitle",
      l10n_util::GetStringUTF16(
          IDS_CHARGER_REPLACEMENT_OFFLINE_ORDER_TITLE));
  localized_strings->SetString(
        "offlineChargerOrderParagraph1",
        l10n_util::GetStringUTF16(
            IDS_CHARGER_REPLACEMENT_OFFLINE_ORDER_P1));
  localized_strings->SetString(
      "offlineChargerOrderParagraph2",
      l10n_util::GetStringUTF16(
          IDS_CHARGER_REPLACEMENT_OFFLINE_ORDER_P2));
  localized_strings->SetString(
      "offlineSafeChargerConfirmationText",
      l10n_util::GetStringUTF16(
          IDS_CHARGER_REPLACEMENT_OFFLINE_ORDER_CONFIRM));
  localized_strings->SetString(
      "privacyPolicy",
      l10n_util::GetStringUTF16(
          IDS_CHARGER_REPLACEMENT_TEXT_PRIVACY_POLICY));
  localized_strings->SetString(
      "offlineOrderPhoneNumber",
      l10n_util::GetStringUTF16(
          IDS_CHARGER_REPLACEMENT_OFFLINE_ORDER_PHONE_NUMBER));
  localized_strings->SetString(
      "offlineOrderPhoneNumber",
      l10n_util::GetStringUTF16(
          IDS_CHARGER_REPLACEMENT_OFFLINE_ORDER_PHONE_NUMBER));
  localized_strings->SetString(
      "chargerStillBadTitle",
      l10n_util::GetStringUTF16(
          IDS_CHARGER_REPLACEMENT_CHARGER_STILL_BAD));
  localized_strings->SetString(
      "chargedOrdered",
      l10n_util::GetStringUTF16(
          IDS_CHARGER_REPLACEMENT_CHARGER_ORDERED));
  localized_strings->SetString(
      "findMoreInfo",
      l10n_util::GetStringUTF16(
          IDS_CHARGER_REPLACEMENT_CHARGER_MORE_INFO));
  localized_strings->SetString("faqLink", kFaqLink);
}

// static
void ChargerReplacementHandler::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kSpringChargerCheck, CHARGER_UNKNOWN);
}

// static
ChargerReplacementHandler::SpringChargerStatus
    ChargerReplacementHandler::GetChargerStatusPref() {
  ChargerReplacementHandler::SpringChargerStatus charger_status =
      static_cast<ChargerReplacementHandler::SpringChargerStatus>(
      g_browser_process->local_state()->GetInteger(prefs::kSpringChargerCheck));
  return charger_status;
}

// static
void ChargerReplacementHandler::SetChargerStatusPref(
    SpringChargerStatus status) {
  g_browser_process->local_state()->SetInteger(prefs::kSpringChargerCheck,
                                               status);
}

void ChargerReplacementHandler::ConfirmSafeCharger(
    const base::ListValue* args) {
  content::RecordAction(
        base::UserMetricsAction("ConfirmSafeSpringCharger"));

  SetChargerStatusPref(CONFIRM_SAFE_CHARGER);
  dialog_->set_can_close(true);
}

void ChargerReplacementHandler::ConfirmNotOrderNewCharger(
    const base::ListValue* args) {
  content::RecordAction(
        base::UserMetricsAction("ConfirmNotToOrderSpringCharger"));

  SetChargerStatusPref(CONFIRM_NOT_ORDER_NEW_CHARGER);
  dialog_->set_can_close(true);
}

void ChargerReplacementHandler::ConfirmChargerOrderedOnline(
    const base::ListValue* args) {
  content::RecordAction(
        base::UserMetricsAction("ConfirmOrderSpringChargerOnline"));
  content::RecordAction(
        base::UserMetricsAction("ConfirmOrderSpringCharger"));

  SetChargerStatusPref(CONFIRM_NEW_CHARGER_ORDERED_ONLINE);
  dialog_->set_can_close(true);
}

void ChargerReplacementHandler::ConfirmChargerOrderByPhone(
    const base::ListValue* args) {
  content::RecordAction(
        base::UserMetricsAction("ConfirmOrderSpringChargerByPhone"));
  content::RecordAction(
        base::UserMetricsAction("ConfirmOrderSpringCharger"));

  SetChargerStatusPref(CONFIRM_ORDER_NEW_CHARGER_BY_PHONE);
  dialog_->set_can_close(true);
}

void ChargerReplacementHandler::ConfirmStillUseBadCharger(
    const base::ListValue* args) {
  content::RecordAction(
      base::UserMetricsAction("ConfirmStillUseOriginalChargerAfterOrder"));

  if (GetChargerStatusPref() == CONFIRM_NEW_CHARGER_ORDERED_ONLINE) {
    SetChargerStatusPref(USE_BAD_CHARGER_AFTER_ORDER_ONLINE);
  } else {
    DCHECK(GetChargerStatusPref() == CONFIRM_ORDER_NEW_CHARGER_BY_PHONE);
    SetChargerStatusPref(USE_BAD_CHARGER_AFTER_ORDER_BY_PHONE);
  }
  dialog_->set_can_close(true);
}

void ChargerReplacementHandler::ShowLink(const base::ListValue* args) {
  std::string url = base::UTF16ToUTF8(ExtractStringValue(args));
  ChargerLinkDialog* dialog = new ChargerLinkDialog(charger_window_, url);
  dialog->Show();
}

}  // namespace chromeos
