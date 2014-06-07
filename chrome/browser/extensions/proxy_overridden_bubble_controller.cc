// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/proxy_overridden_bubble_controller.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

// The minimum time to wait (since the extension was installed) before notifying
// the user about it.
const int kDaysSinceInstallMin = 7;

////////////////////////////////////////////////////////////////////////////////
// ProxyOverriddenBubbleDelegate

class ProxyOverriddenBubbleDelegate
    : public ExtensionMessageBubbleController::Delegate {
 public:
  ProxyOverriddenBubbleDelegate(ExtensionService* service, Profile* profile);
  virtual ~ProxyOverriddenBubbleDelegate();

  // ExtensionMessageBubbleController::Delegate methods.
  virtual bool ShouldIncludeExtension(const std::string& extension_id) OVERRIDE;
  virtual void AcknowledgeExtension(
      const std::string& extension_id,
      ExtensionMessageBubbleController::BubbleAction
          user_action) OVERRIDE;
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
  virtual void RestrictToSingleExtension(
      const std::string& extension_id) OVERRIDE;
  virtual void LogExtensionCount(size_t count) OVERRIDE;
  virtual void LogAction(
      ExtensionMessageBubbleController::BubbleAction
          action) OVERRIDE;

 private:
  // Our extension service. Weak, not owned by us.
  ExtensionService* service_;

  // A weak pointer to the profile we are associated with. Not owned by us.
  Profile* profile_;

  // The ID of the extension we are showing the bubble for.
  std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(ProxyOverriddenBubbleDelegate);
};

ProxyOverriddenBubbleDelegate::ProxyOverriddenBubbleDelegate(
    ExtensionService* service,
    Profile* profile)
    : service_(service), profile_(profile) {}

ProxyOverriddenBubbleDelegate::~ProxyOverriddenBubbleDelegate() {}

bool ProxyOverriddenBubbleDelegate::ShouldIncludeExtension(
    const std::string& extension_id) {
  if (!extension_id_.empty() && extension_id_ != extension_id)
    return false;

  const Extension* extension =
      ExtensionRegistry::Get(profile_)->enabled_extensions().GetByID(
          extension_id);
  if (!extension)
    return false;  // The extension provided is no longer enabled.

  const Extension* overriding = GetExtensionOverridingProxy(profile_);
  if (!overriding || overriding->id() != extension_id)
    return false;

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile_);
  base::TimeDelta since_install =
      base::Time::Now() - prefs->GetInstallTime(extension->id());
  if (since_install.InDays() < kDaysSinceInstallMin)
    return false;

  if (ExtensionPrefs::Get(profile_)->HasProxyOverriddenBubbleBeenAcknowledged(
      extension_id))
    return false;

  return true;
}

void ProxyOverriddenBubbleDelegate::AcknowledgeExtension(
    const std::string& extension_id,
    ExtensionMessageBubbleController::BubbleAction user_action) {
  if (user_action != ExtensionMessageBubbleController::ACTION_EXECUTE) {
    ExtensionPrefs::Get(profile_)->SetProxyOverriddenBubbleBeenAcknowledged(
        extension_id, true);
  }
}

void ProxyOverriddenBubbleDelegate::PerformAction(const ExtensionIdList& list) {
  for (size_t i = 0; i < list.size(); ++i)
    service_->DisableExtension(list[i], Extension::DISABLE_USER_ACTION);
}

void ProxyOverriddenBubbleDelegate::OnClose() {
  ExtensionToolbarModel* toolbar_model =
      ExtensionToolbarModel::Get(profile_);
  if (toolbar_model)
    toolbar_model->StopHighlighting();
}

base::string16 ProxyOverriddenBubbleDelegate::GetTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_EXTENSIONS_PROXY_CONTROLLED_TITLE_HOME_PAGE_BUBBLE);
}

base::string16 ProxyOverriddenBubbleDelegate::GetMessageBody(
    bool anchored_to_browser_action) const {
  if (anchored_to_browser_action) {
    return l10n_util::GetStringUTF16(
        IDS_EXTENSIONS_PROXY_CONTROLLED_FIRST_LINE_EXTENSION_SPECIFIC);
  } else {
    return l10n_util::GetStringUTF16(
        IDS_EXTENSIONS_PROXY_CONTROLLED_FIRST_LINE);
  }
}

base::string16 ProxyOverriddenBubbleDelegate::GetOverflowText(
    const base::string16& overflow_count) const {
  // Does not have more than one extension in the list at a time.
  NOTREACHED();
  return base::string16();
}

base::string16 ProxyOverriddenBubbleDelegate::GetLearnMoreLabel() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

GURL ProxyOverriddenBubbleDelegate::GetLearnMoreUrl() const {
  return GURL(chrome::kExtensionControlledSettingLearnMoreURL);
}

base::string16 ProxyOverriddenBubbleDelegate::GetActionButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_CONTROLLED_RESTORE_SETTINGS);
}

base::string16 ProxyOverriddenBubbleDelegate::GetDismissButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_CONTROLLED_KEEP_CHANGES);
}

bool ProxyOverriddenBubbleDelegate::ShouldShowExtensionList() const {
  return false;
}

void ProxyOverriddenBubbleDelegate::RestrictToSingleExtension(
    const std::string& extension_id) {
  extension_id_ = extension_id;
}

void ProxyOverriddenBubbleDelegate::LogExtensionCount(size_t count) {
  UMA_HISTOGRAM_COUNTS_100("ProxyOverriddenBubble.ExtensionCount", count);
}

void ProxyOverriddenBubbleDelegate::LogAction(
    ExtensionMessageBubbleController::BubbleAction action) {
  UMA_HISTOGRAM_ENUMERATION("ProxyOverriddenBubble.UserSelection",
                            action,
                            ExtensionMessageBubbleController::ACTION_BOUNDARY);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ProxyOverriddenBubbleController

ProxyOverriddenBubbleController::ProxyOverriddenBubbleController(
    Profile* profile)
    : ExtensionMessageBubbleController(
          new ProxyOverriddenBubbleDelegate(
              ExtensionSystem::Get(profile)->extension_service(),
              profile),
          profile),
      profile_(profile) {}

ProxyOverriddenBubbleController::~ProxyOverriddenBubbleController() {}

bool ProxyOverriddenBubbleController::ShouldShow(
    const std::string& extension_id) {
  if (!delegate()->ShouldIncludeExtension(extension_id))
    return false;

  delegate()->RestrictToSingleExtension(extension_id);
  return true;
}

bool ProxyOverriddenBubbleController::CloseOnDeactivate() {
  return true;
}

}  // namespace extensions
