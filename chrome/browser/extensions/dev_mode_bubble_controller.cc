// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/dev_mode_bubble_controller.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_message_bubble.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/feature_switch.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

base::LazyInstance<std::set<Profile*> > g_shown_for_profiles =
    LAZY_INSTANCE_INITIALIZER;

////////////////////////////////////////////////////////////////////////////////
// DevModeBubbleDelegate

DevModeBubbleDelegate::DevModeBubbleDelegate(ExtensionService* service)
    : service_(service) {
}

DevModeBubbleDelegate::~DevModeBubbleDelegate() {
}

bool DevModeBubbleDelegate::ShouldIncludeExtension(
    const std::string& extension_id) {
  const extensions::Extension* extension =
      service_->GetExtensionById(extension_id, false);
  if (!extension)
    return false;
  return extensions::DevModeBubbleController::IsDevModeExtension(extension);
}

void DevModeBubbleDelegate::AcknowledgeExtension(
    const std::string& extension_id,
    ExtensionMessageBubbleController::BubbleAction user_action) {
}

void DevModeBubbleDelegate::PerformAction(
    const extensions::ExtensionIdList& list) {
  for (size_t i = 0; i < list.size(); ++i) {
    service_->DisableExtension(
        list[i], extensions::Extension::DISABLE_USER_ACTION);
  }
}

base::string16 DevModeBubbleDelegate::GetTitle() const {
  return l10n_util::GetStringUTF16(IDS_EXTENSIONS_DISABLE_DEVELOPER_MODE_TITLE);
}

base::string16 DevModeBubbleDelegate::GetMessageBody() const {
  return l10n_util::GetStringUTF16(IDS_EXTENSIONS_DISABLE_DEVELOPER_MODE_BODY);
}

base::string16 DevModeBubbleDelegate::GetOverflowText(
    const base::string16& overflow_count) const {
  return l10n_util::GetStringFUTF16(
            IDS_EXTENSIONS_SUSPICIOUS_DISABLED_AND_N_MORE,
            overflow_count);
}

base::string16 DevModeBubbleDelegate::GetLearnMoreLabel() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
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

bool DevModeBubbleDelegate::ShouldShowExtensionList() const {
  return false;
}

void DevModeBubbleDelegate::LogExtensionCount(size_t count) {
  UMA_HISTOGRAM_COUNTS_100(
      "DevModeExtensionBubble.ExtensionsInDevModeCount", count);
}

void DevModeBubbleDelegate::LogAction(
    ExtensionMessageBubbleController::BubbleAction action) {
  UMA_HISTOGRAM_ENUMERATION(
      "DevModeExtensionBubble.UserSelection",
      action, ExtensionMessageBubbleController::ACTION_BOUNDARY);
}

}  // namespace

namespace extensions {

////////////////////////////////////////////////////////////////////////////////
// DevModeBubbleController

// static
void DevModeBubbleController::ClearProfileListForTesting() {
  g_shown_for_profiles.Get().clear();
}

// static
bool DevModeBubbleController::IsDevModeExtension(
    const Extension* extension) {
  if (!FeatureSwitch::force_dev_mode_highlighting()->IsEnabled()) {
    if (chrome::VersionInfo::GetChannel() <
            chrome::VersionInfo::CHANNEL_BETA)
      return false;
  }
  return extension->location() == Manifest::UNPACKED ||
         extension->location() == Manifest::COMMAND_LINE;
}

DevModeBubbleController::DevModeBubbleController(Profile* profile)
    : ExtensionMessageBubbleController(
          new DevModeBubbleDelegate(
              ExtensionSystem::Get(profile)->extension_service()),
          profile),
      profile_(profile) {}

DevModeBubbleController::~DevModeBubbleController() {
}

bool DevModeBubbleController::ShouldShow() {
  return !g_shown_for_profiles.Get().count(profile_) &&
      !GetExtensionList().empty();
}

void DevModeBubbleController::Show(ExtensionMessageBubble* bubble) {
  g_shown_for_profiles.Get().insert(profile_);
  ExtensionMessageBubbleController::Show(bubble);
}

}  // namespace extensions
