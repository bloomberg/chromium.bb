// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/dev_mode_bubble_controller.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_message_bubble.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "extensions/common/feature_switch.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

static base::LazyInstance<extensions::ProfileKeyedAPIFactory<
    extensions::DevModeBubbleController> >
g_factory = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace extensions {

////////////////////////////////////////////////////////////////////////////////
// DevModeBubbleController

DevModeBubbleController::DevModeBubbleController(
    Profile* profile)
    : ExtensionMessageBubbleController(this, profile),
      service_(extensions::ExtensionSystem::Get(profile)->extension_service()),
      profile_(profile) {
}

DevModeBubbleController::~DevModeBubbleController() {
}

// static
ProfileKeyedAPIFactory<DevModeBubbleController>*
DevModeBubbleController::GetFactoryInstance() {
  return &g_factory.Get();
}

// static
DevModeBubbleController* DevModeBubbleController::Get(
    Profile* profile) {
  return ProfileKeyedAPIFactory<
      DevModeBubbleController>::GetForProfile(profile);
}

bool DevModeBubbleController::IsDevModeExtension(
    const Extension* extension) const {
  if (!extensions::FeatureSwitch::force_dev_mode_highlighting()->IsEnabled()) {
    if (chrome::VersionInfo::GetChannel() <
            chrome::VersionInfo::CHANNEL_BETA)
      return false;
  }
  return extension->location() == Manifest::UNPACKED ||
         extension->location() == Manifest::COMMAND_LINE;
}

bool DevModeBubbleController::ShouldIncludeExtension(
    const std::string& extension_id) {
  const Extension* extension = service_->GetExtensionById(extension_id, false);
  if (!extension)
    return false;
  return IsDevModeExtension(extension);
}

void DevModeBubbleController::AcknowledgeExtension(
    const std::string& extension_id,
    ExtensionMessageBubbleController::BubbleAction user_action) {
}

void DevModeBubbleController::PerformAction(
    const ExtensionIdList& list) {
  for (size_t i = 0; i < list.size(); ++i)
    service_->DisableExtension(list[i], Extension::DISABLE_USER_ACTION);
}

string16 DevModeBubbleController::GetTitle() const {
  return l10n_util::GetStringUTF16(IDS_EXTENSIONS_DISABLE_DEVELOPER_MODE_TITLE);
}

string16 DevModeBubbleController::GetMessageBody() const {
  return l10n_util::GetStringUTF16(IDS_EXTENSIONS_DISABLE_DEVELOPER_MODE_BODY);
}

string16 DevModeBubbleController::GetOverflowText(
    const string16& overflow_count) const {
  return l10n_util::GetStringFUTF16(
            IDS_EXTENSIONS_SUSPICIOUS_DISABLED_AND_N_MORE,
            overflow_count);
}

string16 DevModeBubbleController::GetLearnMoreLabel() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

GURL DevModeBubbleController::GetLearnMoreUrl() const {
  return GURL(chrome::kChromeUIExtensionsURL);
}

string16 DevModeBubbleController::GetActionButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_DISABLE);
}

string16 DevModeBubbleController::GetDismissButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_CANCEL);
}

bool DevModeBubbleController::ShouldShowExtensionList() const {
  return false;
}

std::vector<string16> DevModeBubbleController::GetExtensions() {
  return GetExtensionList();
}

void DevModeBubbleController::LogExtensionCount(size_t count) {
  UMA_HISTOGRAM_COUNTS_100(
      "DevModeExtensionBubble.ExtensionsInDevModeCount", count);
}

void DevModeBubbleController::LogAction(
    ExtensionMessageBubbleController::BubbleAction action) {
  UMA_HISTOGRAM_ENUMERATION(
      "DevModeExtensionBubble.UserSelection",
      action, ExtensionMessageBubbleController::ACTION_BOUNDARY);
}

template <>
void ProfileKeyedAPIFactory<
    DevModeBubbleController>::DeclareFactoryDependencies() {
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
}

}  // namespace extensions
