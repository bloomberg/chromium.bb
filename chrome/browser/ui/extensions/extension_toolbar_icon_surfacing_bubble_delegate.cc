// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_toolbar_icon_surfacing_bubble_delegate.h"

#include "base/logging.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "extensions/common/feature_switch.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

void AcknowledgeInPrefs(PrefService* prefs) {
  prefs->SetBoolean(prefs::kToolbarIconSurfacingBubbleAcknowledged, true);
  // Once the bubble is acknowledged, we no longer need to store the last
  // show time.
  if (prefs->HasPrefPath(prefs::kToolbarIconSurfacingBubbleLastShowTime))
    prefs->ClearPref(prefs::kToolbarIconSurfacingBubbleLastShowTime);
}

}  // namespace

ExtensionToolbarIconSurfacingBubbleDelegate::
ExtensionToolbarIconSurfacingBubbleDelegate(Profile* profile)
    : profile_(profile) {
}

ExtensionToolbarIconSurfacingBubbleDelegate::
~ExtensionToolbarIconSurfacingBubbleDelegate() {
}

bool ExtensionToolbarIconSurfacingBubbleDelegate::ShouldShowForProfile(
    Profile* profile) {
  // If the redesign isn't running, or the user has already acknowledged it,
  // we don't show the bubble.
  PrefService* prefs = profile->GetPrefs();
  if (!extensions::FeatureSwitch::extension_action_redesign()->IsEnabled() ||
      (prefs->HasPrefPath(prefs::kToolbarIconSurfacingBubbleAcknowledged) &&
       prefs->GetBoolean(prefs::kToolbarIconSurfacingBubbleAcknowledged)))
    return false;

  // We don't show more than once per day.
  if (prefs->HasPrefPath(prefs::kToolbarIconSurfacingBubbleLastShowTime)) {
    base::Time last_shown_time = base::Time::FromInternalValue(
        prefs->GetInt64(prefs::kToolbarIconSurfacingBubbleLastShowTime));
    if (base::Time::Now() - last_shown_time < base::TimeDelta::FromDays(1))
      return false;
  }

  if (!ToolbarActionsModel::Get(profile)->RedesignIsShowingNewIcons()) {
    // We only show the bubble if there are any new icons present - otherwise,
    // the user won't see anything different, so we treat it as acknowledged.
    AcknowledgeInPrefs(prefs);
    return false;
  }

  return true;
}

base::string16 ExtensionToolbarIconSurfacingBubbleDelegate::GetHeadingText() {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_TOOLBAR_BUBBLE_HEADING);
}

base::string16 ExtensionToolbarIconSurfacingBubbleDelegate::GetBodyText() {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_TOOLBAR_BUBBLE_CONTENT);
}

base::string16 ExtensionToolbarIconSurfacingBubbleDelegate::GetItemListText() {
  return base::string16();
}

base::string16
ExtensionToolbarIconSurfacingBubbleDelegate::GetActionButtonText() {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_TOOLBAR_BUBBLE_OK);
}

base::string16
ExtensionToolbarIconSurfacingBubbleDelegate::GetDismissButtonText() {
  return base::string16();  // No dismiss button.
}

base::string16
ExtensionToolbarIconSurfacingBubbleDelegate::GetLearnMoreButtonText() {
  return base::string16();  // No learn more link.
}

void ExtensionToolbarIconSurfacingBubbleDelegate::OnBubbleShown() {
  // Record the last time the bubble was shown.
  profile_->GetPrefs()->SetInt64(
      prefs::kToolbarIconSurfacingBubbleLastShowTime,
      base::Time::Now().ToInternalValue());
}

void ExtensionToolbarIconSurfacingBubbleDelegate::OnBubbleClosed(
    CloseAction action) {
  if (action == CLOSE_EXECUTE)
    AcknowledgeInPrefs(profile_->GetPrefs());
  ToolbarActionsModel::Get(profile_)->StopHighlighting();
}
