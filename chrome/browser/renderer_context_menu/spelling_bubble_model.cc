// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/spelling_bubble_model.h"

#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

using content::OpenURLParams;
using content::Referrer;
using content::WebContents;

SpellingBubbleModel::SpellingBubbleModel(Profile* profile,
                                         WebContents* web_contents,
                                         bool include_autocorrect)
    : profile_(profile),
      web_contents_(web_contents),
      include_autocorrect_(include_autocorrect) {
}

SpellingBubbleModel::~SpellingBubbleModel() {
}

base::string16 SpellingBubbleModel::GetTitle() const {
  return l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_SPELLING_ASK_GOOGLE);
}

base::string16 SpellingBubbleModel::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_SPELLING_BUBBLE_TEXT);
}

gfx::Image* SpellingBubbleModel::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_PRODUCT_LOGO_16);
}

base::string16 SpellingBubbleModel::GetButtonLabel(BubbleButton button) const {
  return l10n_util::GetStringUTF16(button == BUTTON_OK ?
      IDS_CONTENT_CONTEXT_SPELLING_BUBBLE_ENABLE :
      IDS_CONTENT_CONTEXT_SPELLING_BUBBLE_DISABLE);
}

void SpellingBubbleModel::Accept() {
  SetPref(true);
}

void SpellingBubbleModel::Cancel() {
  SetPref(false);
}

base::string16 SpellingBubbleModel::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

void SpellingBubbleModel::LinkClicked() {
  OpenURLParams params(
      GURL(chrome::kPrivacyLearnMoreURL), Referrer(), NEW_FOREGROUND_TAB,
      content::PAGE_TRANSITION_LINK, false);
  web_contents_->OpenURL(params);
}

void SpellingBubbleModel::SetPref(bool enabled) {
  PrefService* pref = profile_->GetPrefs();
  DCHECK(pref);
  pref->SetBoolean(prefs::kSpellCheckUseSpellingService, enabled);
  if (include_autocorrect_)
    pref->SetBoolean(prefs::kEnableAutoSpellCorrect, enabled);
}
