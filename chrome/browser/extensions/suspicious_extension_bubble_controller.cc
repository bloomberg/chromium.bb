// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/suspicious_extension_bubble_controller.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_message_bubble.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/user_metrics.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

static base::LazyInstance<extensions::ProfileKeyedAPIFactory<
    extensions::SuspiciousExtensionBubbleController> >
g_factory = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace extensions {

////////////////////////////////////////////////////////////////////////////////
// SuspiciousExtensionBubbleController

SuspiciousExtensionBubbleController::SuspiciousExtensionBubbleController(
    Profile* profile)
    : ExtensionMessageBubbleController(this, profile),
      service_(extensions::ExtensionSystem::Get(profile)->extension_service()),
      profile_(profile) {
}

SuspiciousExtensionBubbleController::
    ~SuspiciousExtensionBubbleController() {
}

// static
ProfileKeyedAPIFactory<SuspiciousExtensionBubbleController>*
SuspiciousExtensionBubbleController::GetFactoryInstance() {
  return &g_factory.Get();
}

// static
SuspiciousExtensionBubbleController*
SuspiciousExtensionBubbleController::Get(Profile* profile) {
  return ProfileKeyedAPIFactory<
      SuspiciousExtensionBubbleController>::GetForProfile(profile);
}

bool SuspiciousExtensionBubbleController::ShouldIncludeExtension(
      const std::string& extension_id) {
  ExtensionPrefs* prefs = service_->extension_prefs();
  if (!prefs->IsExtensionDisabled(extension_id))
    return false;

  int disble_reasons = prefs->GetDisableReasons(extension_id);
  if (disble_reasons & Extension::DISABLE_NOT_VERIFIED)
    return !prefs->HasWipeoutBeenAcknowledged(extension_id);

  return false;
}

void SuspiciousExtensionBubbleController::AcknowledgeExtension(
    const std::string& extension_id,
    ExtensionMessageBubbleController::BubbleAction user_action) {
  ExtensionPrefs* prefs = service_->extension_prefs();
  prefs->SetWipeoutAcknowledged(extension_id, true);
}

void SuspiciousExtensionBubbleController::PerformAction(
    const ExtensionIdList& list) {
  // This bubble solicits no action from the user. Or as Nimoy would have it:
  // "Well, my work here is done".
}

base::string16 SuspiciousExtensionBubbleController::GetTitle() const {
  return l10n_util::GetStringUTF16(IDS_EXTENSIONS_SUSPICIOUS_DISABLED_TITLE);
}

base::string16 SuspiciousExtensionBubbleController::GetMessageBody() const {
  return l10n_util::GetStringFUTF16(IDS_EXTENSIONS_SUSPICIOUS_DISABLED_BODY,
      l10n_util::GetStringUTF16(IDS_EXTENSION_WEB_STORE_TITLE));
}

base::string16 SuspiciousExtensionBubbleController::GetOverflowText(
    const base::string16& overflow_count) const {
  base::string16 overflow_string = l10n_util::GetStringUTF16(
      IDS_EXTENSIONS_SUSPICIOUS_DISABLED_AND_N_MORE);
  base::string16 new_string;

  // Just before string freeze, we checked in # as a substitution value for
  // this string, whereas we should have used $1. It was discovered too late,
  // so we do the substitution by hand in that case.
  if (overflow_string.find(ASCIIToUTF16("#")) != base::string16::npos) {
    base::ReplaceChars(overflow_string, ASCIIToUTF16("#").c_str(),
                       overflow_count, &new_string);
  } else {
    new_string = l10n_util::GetStringFUTF16(
            IDS_EXTENSIONS_SUSPICIOUS_DISABLED_AND_N_MORE,
            overflow_count);
  }

  return new_string;
}

base::string16
SuspiciousExtensionBubbleController::GetLearnMoreLabel() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

GURL SuspiciousExtensionBubbleController::GetLearnMoreUrl() const {
  return GURL(chrome::kRemoveNonCWSExtensionURL);
}

base::string16
SuspiciousExtensionBubbleController::GetActionButtonLabel() const {
  return string16();
}

base::string16
SuspiciousExtensionBubbleController::GetDismissButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_EXTENSIONS_SUSPICIOUS_DISABLED_BUTTON);
}

bool
SuspiciousExtensionBubbleController::ShouldShowExtensionList() const {
  return true;
}

std::vector<string16>
SuspiciousExtensionBubbleController::GetExtensions() {
  return GetExtensionList();
}

void SuspiciousExtensionBubbleController::LogExtensionCount(
    size_t count) {
  UMA_HISTOGRAM_COUNTS_100(
      "ExtensionWipeoutBubble.ExtensionWipeoutCount", count);
}

void SuspiciousExtensionBubbleController::LogAction(
    ExtensionMessageBubbleController::BubbleAction action) {
  UMA_HISTOGRAM_ENUMERATION(
      "ExtensionWipeoutBubble.UserSelection",
      action, ExtensionMessageBubbleController::ACTION_BOUNDARY);
}

template <>
void ProfileKeyedAPIFactory<
    SuspiciousExtensionBubbleController>::DeclareFactoryDependencies() {
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
}

}  // namespace extensions
