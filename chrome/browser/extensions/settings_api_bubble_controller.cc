// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/settings_api_bubble_controller.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/common/extensions/manifest_handlers/settings_overrides_handler.h"
#include "chrome/common/url_constants.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

////////////////////////////////////////////////////////////////////////////////
// SettingsApiBubbleDelegate

class SettingsApiBubbleDelegate
    : public ExtensionMessageBubbleController::Delegate {
 public:
  explicit SettingsApiBubbleDelegate(ExtensionService* service,
                                     Profile* profile,
                                     SettingsApiOverrideType type);
  virtual ~SettingsApiBubbleDelegate();

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
  // Our extension service. Weak, not owned by us.
  ExtensionService* service_;

  // A weak pointer to the profile we are associated with. Not owned by us.
  Profile* profile_;

  // The type of settings override this bubble will report on. This can be, for
  // example, a bubble to notify the user that the search engine has been
  // changed by an extension (or homepage/startup pages/etc).
  SettingsApiOverrideType type_;

  // The ID of the extension we are showing the bubble for.
  std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(SettingsApiBubbleDelegate);
};

SettingsApiBubbleDelegate::SettingsApiBubbleDelegate(
    ExtensionService* service,
    Profile* profile,
    SettingsApiOverrideType type)
    : service_(service), profile_(profile), type_(type) {}

SettingsApiBubbleDelegate::~SettingsApiBubbleDelegate() {}

bool SettingsApiBubbleDelegate::ShouldIncludeExtension(
    const std::string& extension_id) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  const Extension* extension =
      registry->GetExtensionById(extension_id, ExtensionRegistry::ENABLED);
  if (!extension)
    return false;  // The extension provided is no longer enabled.

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile_);
  if (prefs->HasSettingsApiBubbleBeenAcknowledged(extension_id))
    return false;

  const Extension* override = NULL;
  switch (type_) {
    case extensions::BUBBLE_TYPE_HOME_PAGE:
      override = extensions::GetExtensionOverridingHomepage(profile_);
      break;
    case extensions::BUBBLE_TYPE_STARTUP_PAGES:
      override = extensions::GetExtensionOverridingStartupPages(profile_);
      break;
    case extensions::BUBBLE_TYPE_SEARCH_ENGINE:
      override = extensions::GetExtensionOverridingSearchEngine(profile_);
      break;
  }

  if (!override || override->id() != extension->id())
    return false;

  extension_id_ = extension_id;
  return true;
}

void SettingsApiBubbleDelegate::AcknowledgeExtension(
    const std::string& extension_id,
    ExtensionMessageBubbleController::BubbleAction user_action) {
  if (user_action != ExtensionMessageBubbleController::ACTION_EXECUTE) {
    ExtensionPrefs* prefs = ExtensionPrefs::Get(profile_);
    prefs->SetSettingsApiBubbleBeenAcknowledged(extension_id, true);
  }
}

void SettingsApiBubbleDelegate::PerformAction(const ExtensionIdList& list) {
  for (size_t i = 0; i < list.size(); ++i) {
    service_->DisableExtension(list[i], Extension::DISABLE_USER_ACTION);
  }
}

void SettingsApiBubbleDelegate::OnClose() {
  ExtensionToolbarModel* toolbar_model = ExtensionToolbarModel::Get(profile_);
  if (toolbar_model)
    toolbar_model->StopHighlighting();
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
    bool anchored_to_browser_action) const {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  const Extension* extension =
      registry->GetExtensionById(extension_id_, ExtensionRegistry::ENABLED);
  const SettingsOverrides* settings =
      extension ? SettingsOverrides::Get(extension) : NULL;
  if (!extension || !settings) {
    NOTREACHED();
    return base::string16();
  }

  bool home_change = settings->homepage != NULL;
  bool startup_change = !settings->startup_pages.empty();
  bool search_change = settings->search_engine != NULL;

  base::string16 body;
  switch (type_) {
    case BUBBLE_TYPE_HOME_PAGE:
      body = l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_SETTINGS_API_FIRST_LINE_HOME_PAGE);
      if (startup_change && search_change) {
        body += l10n_util::GetStringUTF16(
            IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_START_AND_SEARCH);
      } else if (startup_change) {
        body += l10n_util::GetStringUTF16(
            IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_START_PAGES);
      } else if (search_change) {
        body += l10n_util::GetStringUTF16(
            IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_SEARCH_ENGINE);
      }
      break;
    case BUBBLE_TYPE_STARTUP_PAGES:
      body = l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_SETTINGS_API_FIRST_LINE_START_PAGES);
      if (home_change && search_change) {
        body += l10n_util::GetStringUTF16(
            IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_HOME_AND_SEARCH);
      } else if (home_change) {
        body += l10n_util::GetStringUTF16(
            IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_HOME_PAGE);
      } else if (search_change) {
        body += l10n_util::GetStringUTF16(
            IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_SEARCH_ENGINE);
      }
      break;
    case BUBBLE_TYPE_SEARCH_ENGINE:
      body = l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_SETTINGS_API_FIRST_LINE_SEARCH_ENGINE);
      if (startup_change && home_change) {
        body += l10n_util::GetStringUTF16(
            IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_START_AND_HOME);
      } else if (startup_change) {
        body += l10n_util::GetStringUTF16(
            IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_START_PAGES);
      } else if (home_change) {
        body += l10n_util::GetStringUTF16(
            IDS_EXTENSIONS_SETTINGS_API_SECOND_LINE_HOME_PAGE);
      }
      break;
  }
  if (!body.empty())
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

base::string16 SettingsApiBubbleDelegate::GetLearnMoreLabel() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
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
    Profile* profile,
    SettingsApiOverrideType type)
    : ExtensionMessageBubbleController(
          new SettingsApiBubbleDelegate(
              ExtensionSystem::Get(profile)->extension_service(),
              profile,
              type),
          profile),
      profile_(profile),
      type_(type) {}

SettingsApiBubbleController::~SettingsApiBubbleController() {}

bool SettingsApiBubbleController::ShouldShow(const std::string& extension_id) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile_);
  if (prefs->HasSettingsApiBubbleBeenAcknowledged(extension_id))
    return false;

  if (!delegate()->ShouldIncludeExtension(extension_id))
    return false;

  // If the browser is showing the 'Chrome crashed' infobar, it won't be showing
  // the startup pages, so there's no point in showing the bubble now.
  if (type_ == BUBBLE_TYPE_STARTUP_PAGES)
    return profile_->GetLastSessionExitType() != Profile::EXIT_CRASHED;

  return true;
}

bool SettingsApiBubbleController::CloseOnDeactivate() {
  // Startup bubbles tend to get lost in the focus storm that happens on
  // startup. Other types should dismiss on focus loss.
  return type_ != BUBBLE_TYPE_STARTUP_PAGES;
}

}  // namespace extensions
