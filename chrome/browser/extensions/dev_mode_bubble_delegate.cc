// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/dev_mode_bubble_delegate.h"

#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

DevModeBubbleDelegate::DevModeBubbleDelegate(Profile* profile)
    : ExtensionMessageBubbleController::Delegate(profile) {
}

DevModeBubbleDelegate::~DevModeBubbleDelegate() {
}

bool DevModeBubbleDelegate::ShouldIncludeExtension(const Extension* extension) {
  return (extension->location() == Manifest::UNPACKED ||
          extension->location() == Manifest::COMMAND_LINE);
}

void DevModeBubbleDelegate::AcknowledgeExtension(
    const std::string& extension_id,
    ExtensionMessageBubbleController::BubbleAction user_action) {
}

void DevModeBubbleDelegate::PerformAction(const ExtensionIdList& list) {
  for (size_t i = 0; i < list.size(); ++i)
    service()->DisableExtension(list[i], Extension::DISABLE_USER_ACTION);
}

base::string16 DevModeBubbleDelegate::GetTitle() const {
  return l10n_util::GetStringUTF16(IDS_EXTENSIONS_DISABLE_DEVELOPER_MODE_TITLE);
}

base::string16 DevModeBubbleDelegate::GetMessageBody(
    bool anchored_to_browser_action,
    int extension_count) const {
  return l10n_util::GetStringUTF16(IDS_EXTENSIONS_DISABLE_DEVELOPER_MODE_BODY);
}

base::string16 DevModeBubbleDelegate::GetOverflowText(
    const base::string16& overflow_count) const {
  return l10n_util::GetStringFUTF16(
            IDS_EXTENSIONS_DISABLED_AND_N_MORE,
            overflow_count);
}

GURL DevModeBubbleDelegate::GetLearnMoreUrl() const {
  return GURL(chrome::kChromeUIExtensionsURL);
}

base::string16 DevModeBubbleDelegate::GetActionButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_DISABLE);
}

base::string16 DevModeBubbleDelegate::GetDismissButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_CANCEL);
}

bool DevModeBubbleDelegate::ShouldCloseOnDeactivate() const {
  return false;
}

bool DevModeBubbleDelegate::ShouldAcknowledgeOnDeactivate() const {
  return false;
}

bool DevModeBubbleDelegate::ShouldShowExtensionList() const {
  return false;
}

bool DevModeBubbleDelegate::ShouldHighlightExtensions() const {
  return true;
}

bool DevModeBubbleDelegate::ShouldLimitToEnabledExtensions() const {
  return true;
}

void DevModeBubbleDelegate::LogExtensionCount(size_t count) {
  UMA_HISTOGRAM_COUNTS_100(
      "ExtensionBubble.ExtensionsInDevModeCount", count);
}

void DevModeBubbleDelegate::LogAction(
    ExtensionMessageBubbleController::BubbleAction action) {
  UMA_HISTOGRAM_ENUMERATION(
      "ExtensionBubble.DevModeUserSelection",
      action, ExtensionMessageBubbleController::ACTION_BOUNDARY);
}

const char* DevModeBubbleDelegate::GetKey() {
  return "DevModeBubbleDelegate";
}

bool DevModeBubbleDelegate::ClearProfileSetAfterAction() {
  return false;
}

bool DevModeBubbleDelegate::SupportsPolicyIndicator() {
  return false;
}

}  // namespace extensions
