// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/ntp_overridden_bubble_controller.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
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
  explicit NtpOverriddenBubbleDelegate(Profile* profile);
  ~NtpOverriddenBubbleDelegate() override;

  // ExtensionMessageBubbleController::Delegate methods.
  bool ShouldIncludeExtension(const extensions::Extension* extension) override;
  void AcknowledgeExtension(
      const std::string& extension_id,
      extensions::ExtensionMessageBubbleController::BubbleAction user_action)
      override;
  void PerformAction(const extensions::ExtensionIdList& list) override;
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
  bool ShouldLimitToEnabledExtensions() const override;
  void LogExtensionCount(size_t count) override;
  void LogAction(extensions::ExtensionMessageBubbleController::BubbleAction
                     action) override;

 private:
  // The ID of the extension we are showing the bubble for.
  std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(NtpOverriddenBubbleDelegate);
};

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

  if (extension->id() != url.host())
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

}  // namespace

namespace extensions {

////////////////////////////////////////////////////////////////////////////////
// NtpOverriddenBubbleController

NtpOverriddenBubbleController::NtpOverriddenBubbleController(Browser* browser)
    : ExtensionMessageBubbleController(
          new NtpOverriddenBubbleDelegate(browser->profile()),
          browser) {}

NtpOverriddenBubbleController::~NtpOverriddenBubbleController() {}

bool NtpOverriddenBubbleController::CloseOnDeactivate() {
  return true;
}

}  // namespace extensions
