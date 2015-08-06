// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/proxy_overridden_bubble_controller.h"

#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

// The minimum time to wait (since the extension was installed) before notifying
// the user about it.
const int kDaysSinceInstallMin = 7;

// Whether the user has been notified about extension overriding the proxy.
const char kProxyBubbleAcknowledged[] = "ack_proxy_bubble";

////////////////////////////////////////////////////////////////////////////////
// ProxyOverriddenBubbleDelegate

class ProxyOverriddenBubbleDelegate
    : public ExtensionMessageBubbleController::Delegate {
 public:
  explicit ProxyOverriddenBubbleDelegate(Profile* profile);
  ~ProxyOverriddenBubbleDelegate() override;

  // ExtensionMessageBubbleController::Delegate methods.
  bool ShouldIncludeExtension(const std::string& extension_id) override;
  void AcknowledgeExtension(
      const std::string& extension_id,
      ExtensionMessageBubbleController::BubbleAction user_action) override;
  void PerformAction(const ExtensionIdList& list) override;
  base::string16 GetTitle() const override;
  base::string16 GetMessageBody(bool anchored_to_browser_action,
                                int extension_count) const override;
  base::string16 GetOverflowText(
      const base::string16& overflow_count) const override;
  GURL GetLearnMoreUrl() const override;
  base::string16 GetActionButtonLabel() const override;
  base::string16 GetDismissButtonLabel() const override;
  bool ShouldShowExtensionList() const override;
  bool ShouldHighlightExtensions() const override;
  void RestrictToSingleExtension(const std::string& extension_id) override;
  void LogExtensionCount(size_t count) override;
  void LogAction(
      ExtensionMessageBubbleController::BubbleAction action) override;

 private:
  // The ID of the extension we are showing the bubble for.
  std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(ProxyOverriddenBubbleDelegate);
};

ProxyOverriddenBubbleDelegate::ProxyOverriddenBubbleDelegate(
    Profile* profile)
    : ExtensionMessageBubbleController::Delegate(profile) {
  set_acknowledged_flag_pref_name(kProxyBubbleAcknowledged);
}

ProxyOverriddenBubbleDelegate::~ProxyOverriddenBubbleDelegate() {}

bool ProxyOverriddenBubbleDelegate::ShouldIncludeExtension(
    const std::string& extension_id) {
  if (!extension_id_.empty() && extension_id_ != extension_id)
    return false;

  const Extension* extension =
      registry()->enabled_extensions().GetByID(extension_id);
  if (!extension)
    return false;  // The extension provided is no longer enabled.

  const Extension* overriding = GetExtensionOverridingProxy(profile());
  if (!overriding || overriding->id() != extension_id)
    return false;

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  base::TimeDelta since_install =
      base::Time::Now() - prefs->GetInstallTime(extension->id());
  if (since_install.InDays() < kDaysSinceInstallMin)
    return false;

  if (HasBubbleInfoBeenAcknowledged(extension_id))
    return false;

  return true;
}

void ProxyOverriddenBubbleDelegate::AcknowledgeExtension(
    const std::string& extension_id,
    ExtensionMessageBubbleController::BubbleAction user_action) {
  if (user_action != ExtensionMessageBubbleController::ACTION_EXECUTE)
    SetBubbleInfoBeenAcknowledged(extension_id, true);
}

void ProxyOverriddenBubbleDelegate::PerformAction(const ExtensionIdList& list) {
  for (size_t i = 0; i < list.size(); ++i)
    service()->DisableExtension(list[i], Extension::DISABLE_USER_ACTION);
}

base::string16 ProxyOverriddenBubbleDelegate::GetTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_EXTENSIONS_PROXY_CONTROLLED_TITLE_HOME_PAGE_BUBBLE);
}

base::string16 ProxyOverriddenBubbleDelegate::GetMessageBody(
    bool anchored_to_browser_action,
    int extension_count) const {
  if (anchored_to_browser_action) {
    return l10n_util::GetStringUTF16(
        IDS_EXTENSIONS_PROXY_CONTROLLED_FIRST_LINE_EXTENSION_SPECIFIC);
  } else {
    const Extension* extension = registry()->GetExtensionById(
            extension_id_, ExtensionRegistry::EVERYTHING);
    // If the bubble is about to show, the extension should certainly exist.
    CHECK(extension);
    return l10n_util::GetStringFUTF16(
        IDS_EXTENSIONS_PROXY_CONTROLLED_FIRST_LINE,
        base::UTF8ToUTF16(extension->name()));
  }
}

base::string16 ProxyOverriddenBubbleDelegate::GetOverflowText(
    const base::string16& overflow_count) const {
  // Does not have more than one extension in the list at a time.
  NOTREACHED();
  return base::string16();
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

bool ProxyOverriddenBubbleDelegate::ShouldHighlightExtensions() const {
  return true;
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
    Browser* browser)
    : ExtensionMessageBubbleController(
          new ProxyOverriddenBubbleDelegate(browser->profile()),
          browser) {}

ProxyOverriddenBubbleController::~ProxyOverriddenBubbleController() {}

bool ProxyOverriddenBubbleController::ShouldShow(
    const std::string& extension_id) {
  if (!delegate()->ShouldIncludeExtension(extension_id))
    return false;

  delegate()->RestrictToSingleExtension(extension_id);
  return true;
}

bool ProxyOverriddenBubbleController::CloseOnDeactivate() {
  return false;
}

}  // namespace extensions
