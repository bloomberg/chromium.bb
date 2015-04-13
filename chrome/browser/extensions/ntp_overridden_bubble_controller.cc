// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/ntp_overridden_bubble_controller.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

using extensions::ExtensionMessageBubbleController;
using extensions::NtpOverriddenBubbleController;

namespace {

// Whether the user has been notified about extension overriding the new tab
// page.
const char kNtpBubbleAcknowledged[] = "ack_ntp_bubble";

////////////////////////////////////////////////////////////////////////////////
// NtpOverriddenBubbleDelegate

class NtpOverriddenBubbleDelegate
    : public extensions::ExtensionMessageBubbleController::Delegate {
 public:
  NtpOverriddenBubbleDelegate(ExtensionService* service, Profile* profile);
  ~NtpOverriddenBubbleDelegate() override;

  // ExtensionMessageBubbleController::Delegate methods.
  bool ShouldIncludeExtension(const std::string& extension_id) override;
  void AcknowledgeExtension(
      const std::string& extension_id,
      extensions::ExtensionMessageBubbleController::BubbleAction user_action)
      override;
  void PerformAction(const extensions::ExtensionIdList& list) override;
  base::string16 GetTitle() const override;
  base::string16 GetMessageBody(bool anchored_to_browser_action) const override;
  base::string16 GetOverflowText(
      const base::string16& overflow_count) const override;
  GURL GetLearnMoreUrl() const override;
  base::string16 GetActionButtonLabel() const override;
  base::string16 GetDismissButtonLabel() const override;
  bool ShouldShowExtensionList() const override;
  void RestrictToSingleExtension(const std::string& extension_id) override;
  void LogExtensionCount(size_t count) override;
  void LogAction(extensions::ExtensionMessageBubbleController::BubbleAction
                     action) override;

 private:
  // Our extension service. Weak, not owned by us.
  ExtensionService* service_;

  // The ID of the extension we are showing the bubble for.
  std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(NtpOverriddenBubbleDelegate);
};

NtpOverriddenBubbleDelegate::NtpOverriddenBubbleDelegate(
    ExtensionService* service,
    Profile* profile)
    : extensions::ExtensionMessageBubbleController::Delegate(profile),
      service_(service) {
  set_acknowledged_flag_pref_name(kNtpBubbleAcknowledged);
}

NtpOverriddenBubbleDelegate::~NtpOverriddenBubbleDelegate() {}

bool NtpOverriddenBubbleDelegate::ShouldIncludeExtension(
    const std::string& extension_id) {
  if (!extension_id_.empty() && extension_id_ != extension_id)
    return false;

  using extensions::ExtensionRegistry;
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  const extensions::Extension* extension =
      registry->GetExtensionById(extension_id, ExtensionRegistry::ENABLED);
  if (!extension)
    return false;  // The extension provided is no longer enabled.

  if (HasBubbleInfoBeenAcknowledged(extension_id))
    return false;

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
    service_->DisableExtension(list[i],
                               extensions::Extension::DISABLE_USER_ACTION);
  }
}

base::string16 NtpOverriddenBubbleDelegate::GetTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_EXTENSIONS_NTP_CONTROLLED_TITLE_HOME_PAGE_BUBBLE);
}

base::string16 NtpOverriddenBubbleDelegate::GetMessageBody(
    bool anchored_to_browser_action) const {
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

bool NtpOverriddenBubbleDelegate::ShouldShowExtensionList() const {
  return false;
}

void NtpOverriddenBubbleDelegate::RestrictToSingleExtension(
    const std::string& extension_id) {
  extension_id_ = extension_id;
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

}  // namespace

namespace extensions {

////////////////////////////////////////////////////////////////////////////////
// NtpOverriddenBubbleController

NtpOverriddenBubbleController::NtpOverriddenBubbleController(Profile* profile)
    : ExtensionMessageBubbleController(
          new NtpOverriddenBubbleDelegate(
              ExtensionSystem::Get(profile)->extension_service(),
              profile),
          profile),
      profile_(profile) {}

NtpOverriddenBubbleController::~NtpOverriddenBubbleController() {}

bool NtpOverriddenBubbleController::ShouldShow(
    const std::string& extension_id) {
  if (!delegate()->ShouldIncludeExtension(extension_id))
    return false;

  delegate()->RestrictToSingleExtension(extension_id);
  return true;
}

bool NtpOverriddenBubbleController::CloseOnDeactivate() {
  return true;
}

}  // namespace extensions
