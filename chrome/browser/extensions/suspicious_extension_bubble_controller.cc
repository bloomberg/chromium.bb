// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/suspicious_extension_bubble_controller.h"

#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_message_bubble.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

using extensions::ExtensionMessageBubbleController;

namespace {

// Whether the user has been notified about extension being wiped out.
const char kWipeoutAcknowledged[] = "ack_wiped";

base::LazyInstance<std::set<Profile*> > g_shown_for_profiles =
  LAZY_INSTANCE_INITIALIZER;

////////////////////////////////////////////////////////////////////////////////
// SuspiciousExtensionBubbleDelegate

class SuspiciousExtensionBubbleDelegate
    : public ExtensionMessageBubbleController::Delegate {
 public:
  explicit SuspiciousExtensionBubbleDelegate(Profile* profile);
  ~SuspiciousExtensionBubbleDelegate() override;

  // ExtensionMessageBubbleController::Delegate methods.
  bool ShouldIncludeExtension(const std::string& extension_id) override;
  void AcknowledgeExtension(
      const std::string& extension_id,
      ExtensionMessageBubbleController::BubbleAction user_action) override;
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
  void LogExtensionCount(size_t count) override;
  void LogAction(
      ExtensionMessageBubbleController::BubbleAction action) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SuspiciousExtensionBubbleDelegate);
};

SuspiciousExtensionBubbleDelegate::SuspiciousExtensionBubbleDelegate(
    Profile* profile)
    : extensions::ExtensionMessageBubbleController::Delegate(profile) {
  set_acknowledged_flag_pref_name(kWipeoutAcknowledged);
}

SuspiciousExtensionBubbleDelegate::~SuspiciousExtensionBubbleDelegate() {
}

bool SuspiciousExtensionBubbleDelegate::ShouldIncludeExtension(
      const std::string& extension_id) {
  extensions::ExtensionPrefs* prefs = extensions::ExtensionPrefs::Get(
      profile());
  if (!prefs->IsExtensionDisabled(extension_id))
    return false;

  int disable_reasons = prefs->GetDisableReasons(extension_id);
  if (disable_reasons & extensions::Extension::DISABLE_NOT_VERIFIED)
    return !HasBubbleInfoBeenAcknowledged(extension_id);

  return false;
}

void SuspiciousExtensionBubbleDelegate::AcknowledgeExtension(
    const std::string& extension_id,
    ExtensionMessageBubbleController::BubbleAction user_action) {
  SetBubbleInfoBeenAcknowledged(extension_id, true);
}

void SuspiciousExtensionBubbleDelegate::PerformAction(
    const extensions::ExtensionIdList& list) {
  // This bubble solicits no action from the user. Or as Nimoy would have it:
  // "Well, my work here is done".
}

base::string16 SuspiciousExtensionBubbleDelegate::GetTitle() const {
  return l10n_util::GetStringUTF16(IDS_EXTENSIONS_UNSUPPORTED_DISABLED_TITLE);
}

base::string16 SuspiciousExtensionBubbleDelegate::GetMessageBody(
    bool anchored_to_browser_action,
    int extension_count) const {
  int message_id = extension_count == 1 ?
      IDS_EXTENSIONS_SINGLE_UNSUPPORTED_DISABLED_BODY :
      IDS_EXTENSIONS_MULTIPLE_UNSUPPORTED_DISABLED_BODY;
  return l10n_util::GetStringFUTF16(
      message_id, l10n_util::GetStringUTF16(IDS_EXTENSION_WEB_STORE_TITLE));
}

base::string16 SuspiciousExtensionBubbleDelegate::GetOverflowText(
    const base::string16& overflow_count) const {
  return l10n_util::GetStringFUTF16(
            IDS_EXTENSIONS_DISABLED_AND_N_MORE,
            overflow_count);
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
  return l10n_util::GetStringUTF16(IDS_EXTENSIONS_UNSUPPORTED_DISABLED_BUTTON);
}

bool SuspiciousExtensionBubbleDelegate::ShouldShowExtensionList() const {
  return true;
}

bool SuspiciousExtensionBubbleDelegate::ShouldHighlightExtensions() const {
  return false;
}

void SuspiciousExtensionBubbleDelegate::LogExtensionCount(
    size_t count) {
  UMA_HISTOGRAM_COUNTS_100(
      "ExtensionBubble.ExtensionWipeoutCount", count);
}

void SuspiciousExtensionBubbleDelegate::LogAction(
    ExtensionMessageBubbleController::BubbleAction action) {
  UMA_HISTOGRAM_ENUMERATION(
      "ExtensionBubble.WipeoutUserSelection",
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
    Browser* browser)
    : ExtensionMessageBubbleController(
          new SuspiciousExtensionBubbleDelegate(browser->profile()),
          browser) {}

SuspiciousExtensionBubbleController::~SuspiciousExtensionBubbleController() {
}

bool SuspiciousExtensionBubbleController::ShouldShow() {
  return !g_shown_for_profiles.Get().count(profile()->GetOriginalProfile()) &&
      !GetExtensionList().empty();
}

void SuspiciousExtensionBubbleController::Show(ExtensionMessageBubble* bubble) {
  g_shown_for_profiles.Get().insert(profile()->GetOriginalProfile());
  ExtensionMessageBubbleController::Show(bubble);
}

}  // namespace extensions
