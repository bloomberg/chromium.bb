// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/spelling_bubble_model.h"

#include "base/logging.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

SpellingBubbleModel::SpellingBubbleModel(Profile* profile)
    : profile_(profile) {
}

SpellingBubbleModel::~SpellingBubbleModel() {
}

string16 SpellingBubbleModel::GetTitle() const {
  return l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_SPELLING_ASK_GOOGLE);
}

string16 SpellingBubbleModel::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_SPELLING_BUBBLE_TEXT);
}

gfx::Image* SpellingBubbleModel::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_PRODUCT_LOGO_16);
}

string16 SpellingBubbleModel::GetButtonLabel(BubbleButton button) const {
  return l10n_util::GetStringUTF16(button == BUTTON_OK ?
      IDS_CONTENT_CONTEXT_SPELLING_BUBBLE_ENABLE :
      IDS_CONTENT_CONTEXT_SPELLING_BUBBLE_DISABLE);
}

void SpellingBubbleModel::Accept() {
  PrefService* pref = profile_->GetPrefs();
  DCHECK(pref);
  pref->SetBoolean(prefs::kSpellCheckUseSpellingService, true);
}

void SpellingBubbleModel::Cancel() {
}

string16 SpellingBubbleModel::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_SPELLING_BUBBLE_LINK);
}

void SpellingBubbleModel::LinkClicked() {
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
  browser->OpenURL(
      google_util::AppendGoogleLocaleParam(GURL(chrome::kPrivacyLearnMoreURL)),
      GURL(), NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_LINK);
}
