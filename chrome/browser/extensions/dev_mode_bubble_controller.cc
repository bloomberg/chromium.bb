// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/dev_mode_bubble_controller.h"

#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_message_bubble.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/url_constants.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/feature_switch.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

base::LazyInstance<std::set<Profile*> > g_shown_for_profiles =
    LAZY_INSTANCE_INITIALIZER;

////////////////////////////////////////////////////////////////////////////////
// DevModeBubbleDelegate

class DevModeBubbleDelegate
    : public ExtensionMessageBubbleController::Delegate {
 public:
  explicit DevModeBubbleDelegate(Profile* profile);
  virtual ~DevModeBubbleDelegate();

  // ExtensionMessageBubbleController::Delegate methods.
  virtual bool ShouldIncludeExtension(const std::string& extension_id) OVERRIDE;
  virtual void AcknowledgeExtension(
      const std::string& extension_id,
      ExtensionMessageBubbleController::BubbleAction user_action) OVERRIDE;
  virtual void PerformAction(const ExtensionIdList& list) OVERRIDE;
  virtual void OnClose() OVERRIDE;
  virtual base::string16 GetTitle() const OVERRIDE;
  virtual base::string16 GetMessageBody(
      bool anchored_to_browser_action) const OVERRIDE;
  virtual base::string16 GetOverflowText(
      const base::string16& overflow_count) const OVERRIDE;
  virtual base::string16 GetLearnMoreLabel() const OVERRIDE;
  virtual GURL GetLearnMoreUrl() const OVERRIDE;
  virtual base::string16 GetActionButtonLabel() const OVERRIDE;
  virtual base::string16 GetDismissButtonLabel() const OVERRIDE;
  virtual bool ShouldShowExtensionList() const OVERRIDE;
  virtual void LogExtensionCount(size_t count) OVERRIDE;
  virtual void LogAction(
      ExtensionMessageBubbleController::BubbleAction action) OVERRIDE;

 private:
  // The associated profile (weak).
  Profile* profile_;

  // Our extension service. Weak, not owned by us.
  ExtensionService* service_;

  DISALLOW_COPY_AND_ASSIGN(DevModeBubbleDelegate);
};

DevModeBubbleDelegate::DevModeBubbleDelegate(Profile* profile)
    : profile_(profile),
      service_(ExtensionSystem::Get(profile)->extension_service()) {}

DevModeBubbleDelegate::~DevModeBubbleDelegate() {
}

bool DevModeBubbleDelegate::ShouldIncludeExtension(
    const std::string& extension_id) {
  const Extension* extension = service_->GetExtensionById(extension_id, false);
  if (!extension)
    return false;
  return DevModeBubbleController::IsDevModeExtension(extension);
}

void DevModeBubbleDelegate::AcknowledgeExtension(
    const std::string& extension_id,
    ExtensionMessageBubbleController::BubbleAction user_action) {
}

void DevModeBubbleDelegate::PerformAction(const ExtensionIdList& list) {
  for (size_t i = 0; i < list.size(); ++i)
    service_->DisableExtension(list[i], Extension::DISABLE_USER_ACTION);
}

void DevModeBubbleDelegate::OnClose() {
  ExtensionToolbarModel* toolbar_model = ExtensionToolbarModel::Get(profile_);
  if (toolbar_model)
    toolbar_model->StopHighlighting();
}

base::string16 DevModeBubbleDelegate::GetTitle() const {
  return l10n_util::GetStringUTF16(IDS_EXTENSIONS_DISABLE_DEVELOPER_MODE_TITLE);
}

base::string16 DevModeBubbleDelegate::GetMessageBody(
    bool anchored_to_browser_action) const {
  return l10n_util::GetStringUTF16(IDS_EXTENSIONS_DISABLE_DEVELOPER_MODE_BODY);
}

base::string16 DevModeBubbleDelegate::GetOverflowText(
    const base::string16& overflow_count) const {
  return l10n_util::GetStringFUTF16(
            IDS_EXTENSIONS_DISABLED_AND_N_MORE,
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
      "ExtensionBubble.ExtensionsInDevModeCount", count);
}

void DevModeBubbleDelegate::LogAction(
    ExtensionMessageBubbleController::BubbleAction action) {
  UMA_HISTOGRAM_ENUMERATION(
      "ExtensionBubble.DevModeUserSelection",
      action, ExtensionMessageBubbleController::ACTION_BOUNDARY);
}

}  // namespace

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
    if (chrome::VersionInfo::GetChannel() < chrome::VersionInfo::CHANNEL_BETA)
      return false;
  }
  return extension->location() == Manifest::UNPACKED ||
         extension->location() == Manifest::COMMAND_LINE;
}

DevModeBubbleController::DevModeBubbleController(Profile* profile)
    : ExtensionMessageBubbleController(new DevModeBubbleDelegate(profile),
                                       profile),
      profile_(profile) {}

DevModeBubbleController::~DevModeBubbleController() {
}

bool DevModeBubbleController::ShouldShow() {
  return !g_shown_for_profiles.Get().count(profile_->GetOriginalProfile()) &&
      !GetExtensionList().empty();
}

void DevModeBubbleController::Show(ExtensionMessageBubble* bubble) {
  g_shown_for_profiles.Get().insert(profile_->GetOriginalProfile());
  ExtensionMessageBubbleController::Show(bubble);
}

}  // namespace extensions
