// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/ntp_overridden_bubble_delegate.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Whether the user has been notified about extension overriding the new tab
// page.
const char kNtpBubbleAcknowledged[] = "ack_ntp_bubble";

}  // namespace

namespace extensions {

NtpOverriddenBubbleDelegate::NtpOverriddenBubbleDelegate(
    Profile* profile)
    : extensions::ExtensionMessageBubbleController::Delegate(profile) {
  set_acknowledged_flag_pref_name(kNtpBubbleAcknowledged);
}

NtpOverriddenBubbleDelegate::~NtpOverriddenBubbleDelegate() {}

bool NtpOverriddenBubbleDelegate::ShouldIncludeExtension(
    const extensions::Extension* extension) {
  if (!extension_id_.empty() && extension_id_ != extension->id())
    return false;

  GURL url(chrome::kChromeUINewTabURL);
  if (!ExtensionWebUI::HandleChromeURLOverride(&url, profile()))
    return false;  // No override for newtab found.

  if (extension->id() != url.host_piece())
    return false;

  if (HasBubbleInfoBeenAcknowledged(extension->id()))
    return false;

  extension_id_ = extension->id();
  return true;
}

void NtpOverriddenBubbleDelegate::AcknowledgeExtension(
    const std::string& extension_id,
    ExtensionMessageBubbleController::BubbleAction user_action) {
  if (user_action != ExtensionMessageBubbleController::ACTION_EXECUTE)
    SetBubbleInfoBeenAcknowledged(extension_id, true);
}

void NtpOverriddenBubbleDelegate::PerformAction(
    const extensions::ExtensionIdList& list) {
  for (size_t i = 0; i < list.size(); ++i) {
    service()->DisableExtension(list[i],
                                extensions::Extension::DISABLE_USER_ACTION);
  }
}

base::string16 NtpOverriddenBubbleDelegate::GetTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_EXTENSIONS_NTP_CONTROLLED_TITLE_HOME_PAGE_BUBBLE);
}

base::string16 NtpOverriddenBubbleDelegate::GetMessageBody(
    bool anchored_to_browser_action,
    int extension_count) const {
  base::string16 body =
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_NTP_CONTROLLED_FIRST_LINE);
  body += l10n_util::GetStringUTF16(
      IDS_EXTENSIONS_SETTINGS_API_THIRD_LINE_CONFIRMATION);
  return body;
}

base::string16 NtpOverriddenBubbleDelegate::GetOverflowText(
    const base::string16& overflow_count) const {
  // Does not have more than one extension in the list at a time.
  NOTREACHED();
  return base::string16();
}

GURL NtpOverriddenBubbleDelegate::GetLearnMoreUrl() const {
  return GURL(chrome::kExtensionControlledSettingLearnMoreURL);
}

base::string16 NtpOverriddenBubbleDelegate::GetActionButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_CONTROLLED_RESTORE_SETTINGS);
}

base::string16 NtpOverriddenBubbleDelegate::GetDismissButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_CONTROLLED_KEEP_CHANGES);
}

bool NtpOverriddenBubbleDelegate::ShouldCloseOnDeactivate() const {
  return true;
}

bool NtpOverriddenBubbleDelegate::ShouldAcknowledgeOnDeactivate() const {
  return base::FeatureList::IsEnabled(
      features::kAcknowledgeNtpOverrideOnDeactivate);
}

bool NtpOverriddenBubbleDelegate::ShouldShowExtensionList() const {
  return false;
}

bool NtpOverriddenBubbleDelegate::ShouldHighlightExtensions() const {
  return false;
}

bool NtpOverriddenBubbleDelegate::ShouldLimitToEnabledExtensions() const {
  return true;
}

void NtpOverriddenBubbleDelegate::LogExtensionCount(size_t count) {
}

void NtpOverriddenBubbleDelegate::LogAction(
    ExtensionMessageBubbleController::BubbleAction action) {
  UMA_HISTOGRAM_ENUMERATION(
      "ExtensionOverrideBubble.NtpOverriddenUserSelection",
      action,
      ExtensionMessageBubbleController::ACTION_BOUNDARY);
}

const char* NtpOverriddenBubbleDelegate::GetKey() {
  return "NtpOverriddenBubbleDelegate";
}

bool NtpOverriddenBubbleDelegate::SupportsPolicyIndicator() {
  return true;
}

}  // namespace extensions
