// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_message_bubble_controller.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/user_metrics.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

// How many extensions to show in the bubble (max).
const int kMaxExtensionsToShow = 7;

// Whether or not to ignore the learn more link navigation for testing.
bool g_should_ignore_learn_more_for_testing = false;

using ProfileSetMap = std::map<std::string, std::set<Profile*>>;
base::LazyInstance<ProfileSetMap> g_shown_for_profiles =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ExtensionMessageBubbleController::Delegate

ExtensionMessageBubbleController::Delegate::Delegate(Profile* profile)
    : profile_(profile),
      service_(ExtensionSystem::Get(profile)->extension_service()),
      registry_(ExtensionRegistry::Get(profile)) {
}

ExtensionMessageBubbleController::Delegate::~Delegate() {
}

base::string16 ExtensionMessageBubbleController::Delegate::GetLearnMoreLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool ExtensionMessageBubbleController::Delegate::ClearProfileSetAfterAction() {
  return true;
}

bool ExtensionMessageBubbleController::Delegate::HasBubbleInfoBeenAcknowledged(
    const std::string& extension_id) {
  std::string pref_name = get_acknowledged_flag_pref_name();
  if (pref_name.empty())
    return false;
  bool pref_state = false;
  extensions::ExtensionPrefs* prefs = extensions::ExtensionPrefs::Get(profile_);
  prefs->ReadPrefAsBoolean(extension_id, pref_name, &pref_state);
  return pref_state;
}

void ExtensionMessageBubbleController::Delegate::SetBubbleInfoBeenAcknowledged(
    const std::string& extension_id,
    bool value) {
  std::string pref_name = get_acknowledged_flag_pref_name();
  if (pref_name.empty())
    return;
  extensions::ExtensionPrefs* prefs = extensions::ExtensionPrefs::Get(profile_);
  prefs->UpdateExtensionPref(extension_id,
                             pref_name,
                             value ? new base::FundamentalValue(value) : NULL);
}

std::string
ExtensionMessageBubbleController::Delegate::get_acknowledged_flag_pref_name()
    const {
  return acknowledged_pref_name_;
}

void ExtensionMessageBubbleController::Delegate::
    set_acknowledged_flag_pref_name(const std::string& pref_name) {
  acknowledged_pref_name_ = pref_name;
}

////////////////////////////////////////////////////////////////////////////////
// ExtensionMessageBubbleController

ExtensionMessageBubbleController::ExtensionMessageBubbleController(
    Delegate* delegate,
    Browser* browser)
    : browser_(browser),
      user_action_(ACTION_BOUNDARY),
      delegate_(delegate),
      initialized_(false),
      did_highlight_(false),
      browser_list_observer_(this) {
  browser_list_observer_.Add(BrowserList::GetInstance());
}

ExtensionMessageBubbleController::~ExtensionMessageBubbleController() {
  if (did_highlight_)
    ToolbarActionsModel::Get(profile())->StopHighlighting();
}

Profile* ExtensionMessageBubbleController::profile() {
  return browser_->profile();
}

bool ExtensionMessageBubbleController::ShouldShow() {
  std::set<Profile*>* profiles = GetProfileSet();
  return !profiles->count(profile()->GetOriginalProfile()) &&
         !GetExtensionList().empty();
}

std::vector<base::string16>
ExtensionMessageBubbleController::GetExtensionList() {
  ExtensionIdList* list = GetOrCreateExtensionList();
  if (list->empty())
    return std::vector<base::string16>();

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  std::vector<base::string16> return_value;
  for (const std::string& id : *list) {
    const Extension* extension =
        registry->GetExtensionById(id, ExtensionRegistry::EVERYTHING);
    // The extension may have been removed, since showing the bubble is an
    // asynchronous process.
    if (extension)
      return_value.push_back(base::UTF8ToUTF16(extension->name()));
  }
  return return_value;
}

base::string16 ExtensionMessageBubbleController::GetExtensionListForDisplay() {
  if (!delegate_->ShouldShowExtensionList())
    return base::string16();

  std::vector<base::string16> extension_list = GetExtensionList();
  if (extension_list.size() > kMaxExtensionsToShow) {
    int old_size = extension_list.size();
    extension_list.erase(extension_list.begin() + kMaxExtensionsToShow,
                         extension_list.end());
    extension_list.push_back(delegate_->GetOverflowText(base::IntToString16(
        old_size - kMaxExtensionsToShow)));
  }
  const base::char16 bullet_point = 0x2022;
  base::string16 prefix = bullet_point + base::ASCIIToUTF16(" ");
  for (base::string16& str : extension_list)
    str.insert(0, prefix);
  return base::JoinString(extension_list, base::ASCIIToUTF16("\n"));
}

const ExtensionIdList& ExtensionMessageBubbleController::GetExtensionIdList() {
  return *GetOrCreateExtensionList();
}

bool ExtensionMessageBubbleController::CloseOnDeactivate() {
  return delegate_->ShouldCloseOnDeactivate();
}

void ExtensionMessageBubbleController::HighlightExtensionsIfNecessary() {
  if (delegate_->ShouldHighlightExtensions() && !did_highlight_) {
    did_highlight_ = true;
    const ExtensionIdList& extension_ids = GetExtensionIdList();
    DCHECK(!extension_ids.empty());
    ToolbarActionsModel::Get(profile())->HighlightActions(
        extension_ids, ToolbarActionsModel::HIGHLIGHT_WARNING);
  }
}

void ExtensionMessageBubbleController::OnShown() {
  GetProfileSet()->insert(profile()->GetOriginalProfile());
}

void ExtensionMessageBubbleController::OnBubbleAction() {
  DCHECK_EQ(ACTION_BOUNDARY, user_action_);
  user_action_ = ACTION_EXECUTE;

  delegate_->LogAction(ACTION_EXECUTE);
  delegate_->PerformAction(*GetOrCreateExtensionList());

  OnClose();
}

void ExtensionMessageBubbleController::OnBubbleDismiss(
    bool closed_by_deactivation) {
  // OnBubbleDismiss() can be called twice when we receive multiple
  // "OnWidgetDestroying" notifications (this can at least happen when we close
  // a window with a notification open). Handle this gracefully.
  if (user_action_ != ACTION_BOUNDARY) {
    DCHECK(user_action_ == ACTION_DISMISS_USER_ACTION ||
           user_action_ == ACTION_DISMISS_DEACTIVATION);
    return;
  }

  user_action_ = closed_by_deactivation ? ACTION_DISMISS_DEACTIVATION
                                        : ACTION_DISMISS_USER_ACTION;

  delegate_->LogAction(user_action_);

  OnClose();
}

void ExtensionMessageBubbleController::OnLinkClicked() {
  DCHECK_EQ(ACTION_BOUNDARY, user_action_);
  user_action_ = ACTION_LEARN_MORE;

  delegate_->LogAction(ACTION_LEARN_MORE);
  if (!g_should_ignore_learn_more_for_testing) {
    browser_->OpenURL(
        content::OpenURLParams(delegate_->GetLearnMoreUrl(),
                               content::Referrer(),
                               NEW_FOREGROUND_TAB,
                               ui::PAGE_TRANSITION_LINK,
                               false));
  }
  OnClose();
}

void ExtensionMessageBubbleController::ClearProfileListForTesting() {
  GetProfileSet()->clear();
}

// static
void ExtensionMessageBubbleController::set_should_ignore_learn_more_for_testing(
    bool should_ignore) {
  g_should_ignore_learn_more_for_testing = should_ignore;
}

void ExtensionMessageBubbleController::OnBrowserRemoved(Browser* browser) {
  if (browser == browser_ && did_highlight_) {
    ToolbarActionsModel::Get(profile())->StopHighlighting();
    did_highlight_ = false;
  }
}

void ExtensionMessageBubbleController::AcknowledgeExtensions() {
  ExtensionIdList* list = GetOrCreateExtensionList();
  for (ExtensionIdList::const_iterator it = list->begin();
       it != list->end(); ++it)
    delegate_->AcknowledgeExtension(*it, user_action_);
}

ExtensionIdList* ExtensionMessageBubbleController::GetOrCreateExtensionList() {
  if (!initialized_) {
    ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
    std::unique_ptr<const ExtensionSet> all_extensions;
    if (!delegate_->ShouldLimitToEnabledExtensions())
      all_extensions = registry->GenerateInstalledExtensionsSet();
    const ExtensionSet& extensions_to_check =
        all_extensions ? *all_extensions : registry->enabled_extensions();
    for (const scoped_refptr<const Extension>& extension :
         extensions_to_check) {
      if (delegate_->ShouldIncludeExtension(extension.get()))
        extension_list_.push_back(extension->id());
    }

    delegate_->LogExtensionCount(extension_list_.size());
    initialized_ = true;
  }

  return &extension_list_;
}

void ExtensionMessageBubbleController::OnClose() {
  DCHECK_NE(ACTION_BOUNDARY, user_action_);
  // If the bubble was closed due to deactivation, don't treat it as
  // acknowledgment so that the user will see the bubble again (until they
  // explicitly take an action).
  if (user_action_ != ACTION_DISMISS_DEACTIVATION) {
    AcknowledgeExtensions();
    if (delegate_->ClearProfileSetAfterAction())
      GetProfileSet()->clear();
  }
}

std::set<Profile*>* ExtensionMessageBubbleController::GetProfileSet() {
  return &g_shown_for_profiles.Get()[delegate_->GetKey()];
}

}  // namespace extensions
