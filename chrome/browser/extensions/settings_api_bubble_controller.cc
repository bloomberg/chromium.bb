// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/settings_api_bubble_controller.h"

#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/common/extensions/manifest_handlers/settings_overrides_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

// Whether the user has been notified about extension taking over some aspect of
// the user's settings (homepage, startup pages, or search engine).
const char kSettingsBubbleAcknowledged[] = "ack_settings_bubble";

////////////////////////////////////////////////////////////////////////////////
// SettingsApiBubbleDelegate

class SettingsApiBubbleDelegate
    : public ExtensionMessageBubbleController::Delegate {
 public:
  SettingsApiBubbleDelegate(Profile* profile, SettingsApiOverrideType type);
  ~SettingsApiBubbleDelegate() override;

  // ExtensionMessageBubbleController::Delegate methods.
  bool ShouldIncludeExtension(const Extension* extension) override;
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
  bool ShouldLimitToEnabledExtensions() const override;
  void LogExtensionCount(size_t count) override;
  void LogAction(
      ExtensionMessageBubbleController::BubbleAction action) override;

 private:
  // The type of settings override this bubble will report on. This can be, for
  // example, a bubble to notify the user that the search engine has been
  // changed by an extension (or homepage/startup pages/etc).
  SettingsApiOverrideType type_;

  // The ID of the extension we are showing the bubble for.
  std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(SettingsApiBubbleDelegate);
};

SettingsApiBubbleDelegate::SettingsApiBubbleDelegate(
    Profile* profile,
    SettingsApiOverrideType type)
    : ExtensionMessageBubbleController::Delegate(profile),
      type_(type) {
  set_acknowledged_flag_pref_name(kSettingsBubbleAcknowledged);
}

SettingsApiBubbleDelegate::~SettingsApiBubbleDelegate() {}

bool SettingsApiBubbleDelegate::ShouldIncludeExtension(
    const Extension* extension) {
  // If the browser is showing the 'Chrome crashed' infobar, it won't be showing
  // the startup pages, so there's no point in showing the bubble now.
  if (type_ == BUBBLE_TYPE_STARTUP_PAGES &&
      profile()->GetLastSessionExitType() == Profile::EXIT_CRASHED)
    return false;

  if (HasBubbleInfoBeenAcknowledged(extension->id()))
    return false;

  const Extension* override = nullptr;
  switch (type_) {
    case extensions::BUBBLE_TYPE_HOME_PAGE:
      override = extensions::GetExtensionOverridingHomepage(profile());
      break;
    case extensions::BUBBLE_TYPE_STARTUP_PAGES:
      override = extensions::GetExtensionOverridingStartupPages(profile());
      break;
    case extensions::BUBBLE_TYPE_SEARCH_ENGINE:
      override = extensions::GetExtensionOverridingSearchEngine(profile());
      break;
  }

  if (!override || override != extension)
    return false;

  extension_id_ = extension->id();
  return true;
}

void SettingsApiBubbleDelegate::AcknowledgeExtension(
    const std::string& extension_id,
    ExtensionMessageBubbleController::BubbleAction user_action) {
  if (user_action != ExtensionMessageBubbleController::ACTION_EXECUTE)
    SetBubbleInfoBeenAcknowledged(extension_id, true);
}

void SettingsApiBubbleDelegate::PerformAction(const ExtensionIdList& list) {
  for (size_t i = 0; i < list.size(); ++i) {
    service()->DisableExtension(list[i], Extension::DISABLE_USER_ACTION);
  }
}

base::string16 SettingsApiBubbleDelegate::GetTitle() const {
  switch (type_) {
    case BUBBLE_TYPE_HOME_PAGE:
      return l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_SETTINGS_API_TITLE_HOME_PAGE_BUBBLE);
    case BUBBLE_TYPE_STARTUP_PAGES:
      return l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_SETTINGS_API_TITLE_STARTUP_PAGES_BUBBLE);
    case BUBBLE_TYPE_SEARCH_ENGINE:
      return l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_SETTINGS_API_TITLE_SEARCH_ENGINE_BUBBLE);
  }
  NOTREACHED();
  return base::string16();
}

base::string16 SettingsApiBubbleDelegate::GetMessageBody(
    bool anchored_to_browser_action,
    int extension_count) const {
  const Extension* extension =
      registry()->GetExtensionById(extension_id_, ExtensionRegistry::ENABLED);
  const SettingsOverrides* settings =
      extension ? SettingsOverrides::Get(extension) : NULL;
  if (!extension || !settings) {
    NOTREACHED();
    return base::string16();
  }

  bool home_change = settings->homepage != NULL;
  bool startup_change = !settings->startup_pages.empty();
  bool search_change = settings->search_engine != NULL;

  int first_line_id = 0;
  int second_line_id = 0;

  base::string16 body;
  switch (type_) {
    case BUBBLE_TYPE_HOME_PAGE:
      first_line_id = anchored_to_browser_action ?
          IDS_EXTENSIONS_SETTINGS_API_FIRST_LINE_HOME_PAGE_SPECIFIC :
          IDS_EXTENSIONS_SETTINGS_API_FIRST_LINE_HOME_PAGE;
      if (startup_change && search_change) {
        second_line_id =
            IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_START_AND_SEARCH;
      } else if (startup_change) {
        second_line_id = IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_START_PAGES;
      } else if (search_change) {
        second_line_id = IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_SEARCH_ENGINE;
      }
      break;
    case BUBBLE_TYPE_STARTUP_PAGES:
      first_line_id = anchored_to_browser_action ?
          IDS_EXTENSIONS_SETTINGS_API_FIRST_LINE_START_PAGES_SPECIFIC :
          IDS_EXTENSIONS_SETTINGS_API_FIRST_LINE_START_PAGES;
      if (home_change && search_change) {
        second_line_id =
            IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_HOME_AND_SEARCH;
      } else if (home_change) {
        second_line_id = IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_HOME_PAGE;
      } else if (search_change) {
        second_line_id = IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_SEARCH_ENGINE;
      }
      break;
    case BUBBLE_TYPE_SEARCH_ENGINE:
      first_line_id = anchored_to_browser_action ?
          IDS_EXTENSIONS_SETTINGS_API_FIRST_LINE_SEARCH_ENGINE_SPECIFIC :
          IDS_EXTENSIONS_SETTINGS_API_FIRST_LINE_SEARCH_ENGINE;
      if (startup_change && home_change)
        second_line_id = IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_START_AND_HOME;
      else if (startup_change)
        second_line_id = IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_START_PAGES;
      else if (home_change)
        second_line_id = IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_HOME_PAGE;
      break;
  }
  DCHECK_NE(0, first_line_id);
  body = anchored_to_browser_action ?
      l10n_util::GetStringUTF16(first_line_id) :
      l10n_util::GetStringFUTF16(first_line_id,
                                 base::UTF8ToUTF16(extension->name()));
  if (second_line_id)
    body += l10n_util::GetStringUTF16(second_line_id);

  body += l10n_util::GetStringUTF16(
      IDS_EXTENSIONS_SETTINGS_API_THIRD_LINE_CONFIRMATION);

  return body;
}

base::string16 SettingsApiBubbleDelegate::GetOverflowText(
    const base::string16& overflow_count) const {
  // Does not have more than one extension in the list at a time.
  NOTREACHED();
  return base::string16();
}

GURL SettingsApiBubbleDelegate::GetLearnMoreUrl() const {
  return GURL(chrome::kExtensionControlledSettingLearnMoreURL);
}

base::string16 SettingsApiBubbleDelegate::GetActionButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_CONTROLLED_RESTORE_SETTINGS);
}

base::string16 SettingsApiBubbleDelegate::GetDismissButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_CONTROLLED_KEEP_CHANGES);
}

bool SettingsApiBubbleDelegate::ShouldShowExtensionList() const {
  return false;
}

bool SettingsApiBubbleDelegate::ShouldHighlightExtensions() const {
  return type_ == BUBBLE_TYPE_STARTUP_PAGES;
}

bool SettingsApiBubbleDelegate::ShouldLimitToEnabledExtensions() const {
  return true;
}

void SettingsApiBubbleDelegate::LogExtensionCount(size_t count) {
}

void SettingsApiBubbleDelegate::LogAction(
    ExtensionMessageBubbleController::BubbleAction action) {
  switch (type_) {
    case BUBBLE_TYPE_HOME_PAGE:
      UMA_HISTOGRAM_ENUMERATION(
          "ExtensionOverrideBubble.SettingsApiUserSelectionHomePage",
          action,
          ExtensionMessageBubbleController::ACTION_BOUNDARY);
      break;
    case BUBBLE_TYPE_STARTUP_PAGES:
      UMA_HISTOGRAM_ENUMERATION(
          "ExtensionOverrideBubble.SettingsApiUserSelectionStartupPage",
          action,
          ExtensionMessageBubbleController::ACTION_BOUNDARY);
      break;
    case BUBBLE_TYPE_SEARCH_ENGINE:
      UMA_HISTOGRAM_ENUMERATION(
          "ExtensionOverrideBubble.SettingsApiUserSelectionSearchEngine",
          action,
          ExtensionMessageBubbleController::ACTION_BOUNDARY);
      break;
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// SettingsApiBubbleController

SettingsApiBubbleController::SettingsApiBubbleController(
    Browser* browser,
    SettingsApiOverrideType type)
    : ExtensionMessageBubbleController(
          new SettingsApiBubbleDelegate(browser->profile(), type),
          browser),
      type_(type) {}

SettingsApiBubbleController::~SettingsApiBubbleController() {}

bool SettingsApiBubbleController::CloseOnDeactivate() {
  // Startup bubbles tend to get lost in the focus storm that happens on
  // startup. Other types should dismiss on focus loss.
  return type_ != BUBBLE_TYPE_STARTUP_PAGES;
}

}  // namespace extensions
