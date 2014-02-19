// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/suspicious_extension_bubble_controller.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_message_bubble.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/user_metrics.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

base::LazyInstance<std::set<Profile*> > g_shown_for_profiles =
  LAZY_INSTANCE_INITIALIZER;

////////////////////////////////////////////////////////////////////////////////
// SuspiciousExtensionBubbleDelegate

SuspiciousExtensionBubbleDelegate::SuspiciousExtensionBubbleDelegate(
    Profile* profile)
    : profile_(profile) {}

SuspiciousExtensionBubbleDelegate::~SuspiciousExtensionBubbleDelegate() {
}

bool SuspiciousExtensionBubbleDelegate::ShouldIncludeExtension(
      const std::string& extension_id) {
  extensions::ExtensionPrefs* prefs = extensions::ExtensionPrefs::Get(profile_);
  if (!prefs->IsExtensionDisabled(extension_id))
    return false;

  int disble_reasons = prefs->GetDisableReasons(extension_id);
  if (disble_reasons & extensions::Extension::DISABLE_NOT_VERIFIED)
    return !prefs->HasWipeoutBeenAcknowledged(extension_id);

  return false;
}

void SuspiciousExtensionBubbleDelegate::AcknowledgeExtension(
    const std::string& extension_id,
    ExtensionMessageBubbleController::BubbleAction user_action) {
  extensions::ExtensionPrefs* prefs = extensions::ExtensionPrefs::Get(profile_);
  prefs->SetWipeoutAcknowledged(extension_id, true);
}

void SuspiciousExtensionBubbleDelegate::PerformAction(
    const extensions::ExtensionIdList& list) {
  // This bubble solicits no action from the user. Or as Nimoy would have it:
  // "Well, my work here is done".
}

base::string16 SuspiciousExtensionBubbleDelegate::GetTitle() const {
  return l10n_util::GetStringUTF16(IDS_EXTENSIONS_SUSPICIOUS_DISABLED_TITLE);
}

base::string16 SuspiciousExtensionBubbleDelegate::GetMessageBody() const {
  return l10n_util::GetStringFUTF16(IDS_EXTENSIONS_SUSPICIOUS_DISABLED_BODY,
      l10n_util::GetStringUTF16(IDS_EXTENSION_WEB_STORE_TITLE));
}

base::string16 SuspiciousExtensionBubbleDelegate::GetOverflowText(
    const base::string16& overflow_count) const {
  base::string16 overflow_string = l10n_util::GetStringUTF16(
      IDS_EXTENSIONS_SUSPICIOUS_DISABLED_AND_N_MORE);
  base::string16 new_string;

  // Just before string freeze, we checked in # as a substitution value for
  // this string, whereas we should have used $1. It was discovered too late,
  // so we do the substitution by hand in that case.
  if (overflow_string.find(base::ASCIIToUTF16("#")) != base::string16::npos) {
    base::ReplaceChars(overflow_string, base::ASCIIToUTF16("#").c_str(),
                       overflow_count, &new_string);
  } else {
    new_string = l10n_util::GetStringFUTF16(
            IDS_EXTENSIONS_SUSPICIOUS_DISABLED_AND_N_MORE,
            overflow_count);
  }

  return new_string;
}

base::string16
SuspiciousExtensionBubbleDelegate::GetLearnMoreLabel() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

GURL SuspiciousExtensionBubbleDelegate::GetLearnMoreUrl() const {
  return GURL(chrome::kRemoveNonCWSExtensionURL);
}

base::string16
SuspiciousExtensionBubbleDelegate::GetActionButtonLabel() const {
  return base::string16();
}

base::string16
SuspiciousExtensionBubbleDelegate::GetDismissButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_EXTENSIONS_SUSPICIOUS_DISABLED_BUTTON);
}

bool
SuspiciousExtensionBubbleDelegate::ShouldShowExtensionList() const {
  return true;
}

void SuspiciousExtensionBubbleDelegate::LogExtensionCount(
    size_t count) {
  UMA_HISTOGRAM_COUNTS_100(
      "ExtensionWipeoutBubble.ExtensionWipeoutCount", count);
}

void SuspiciousExtensionBubbleDelegate::LogAction(
    ExtensionMessageBubbleController::BubbleAction action) {
  UMA_HISTOGRAM_ENUMERATION(
      "ExtensionWipeoutBubble.UserSelection",
      action, ExtensionMessageBubbleController::ACTION_BOUNDARY);
}

}  // namespace

namespace extensions {

////////////////////////////////////////////////////////////////////////////////
// SuspiciousExtensionBubbleController

// static
void SuspiciousExtensionBubbleController::ClearProfileListForTesting() {
  g_shown_for_profiles.Get().clear();
}

SuspiciousExtensionBubbleController::SuspiciousExtensionBubbleController(
    Profile* profile)
    : ExtensionMessageBubbleController(
          new SuspiciousExtensionBubbleDelegate(profile),
          profile),
      profile_(profile) {}

SuspiciousExtensionBubbleController::~SuspiciousExtensionBubbleController() {
}

bool SuspiciousExtensionBubbleController::ShouldShow() {
  return !g_shown_for_profiles.Get().count(profile_) &&
      !GetExtensionList().empty();
}

void SuspiciousExtensionBubbleController::Show(ExtensionMessageBubble* bubble) {
  g_shown_for_profiles.Get().insert(profile_);
  ExtensionMessageBubbleController::Show(bubble);
}

}  // namespace extensions
