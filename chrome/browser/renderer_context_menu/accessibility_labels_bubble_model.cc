// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/accessibility_labels_bubble_model.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

using content::OpenURLParams;
using content::Referrer;
using content::WebContents;

AccessibilityLabelsBubbleModel::AccessibilityLabelsBubbleModel(
    Profile* profile,
    WebContents* web_contents)
    : profile_(profile), web_contents_(web_contents) {}

AccessibilityLabelsBubbleModel::~AccessibilityLabelsBubbleModel() {}

base::string16 AccessibilityLabelsBubbleModel::GetTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_SEND);
}

base::string16 AccessibilityLabelsBubbleModel::GetMessageText() const {
  return l10n_util::GetStringUTF16(
      IDS_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_BUBBLE_TEXT);
}

base::string16 AccessibilityLabelsBubbleModel::GetButtonLabel(
    BubbleButton button) const {
  return l10n_util::GetStringUTF16(
      button == BUTTON_OK
          ? IDS_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_BUBBLE_ENABLE
          : IDS_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_BUBBLE_DISABLE);
}

void AccessibilityLabelsBubbleModel::Accept() {
  // TODO(katie): Add logging.
  SetPref(true);
}

void AccessibilityLabelsBubbleModel::Cancel() {
  // TODO(katie): Add logging.
  SetPref(false);
}

base::string16 AccessibilityLabelsBubbleModel::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

GURL AccessibilityLabelsBubbleModel::GetHelpPageURL() const {
  return GURL(chrome::kPrivacyLearnMoreURL);
}

void AccessibilityLabelsBubbleModel::OpenHelpPage() {
  // TODO(katie): Link to a specific accessibility labels help page when
  // there is one; check with the privacy team.
  OpenURLParams params(GetHelpPageURL(), Referrer(),
                       WindowOpenDisposition::NEW_FOREGROUND_TAB,
                       ui::PAGE_TRANSITION_LINK, false);
  web_contents_->OpenURL(params);
}

void AccessibilityLabelsBubbleModel::SetPref(bool enabled) {
  PrefService* pref = profile_->GetPrefs();
  DCHECK(pref);
  pref->SetBoolean(prefs::kAccessibilityImageLabelsEnabled, enabled);
}
