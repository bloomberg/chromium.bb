// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_resource_manager.h"

#include <memory>

#include "ash/app_list/model/speech/speech_ui_model.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/start_page_service.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "ui/app_list/app_list_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace app_list {

SearchResourceManager::SearchResourceManager(Profile* profile,
                                             AppListModelUpdater* model_updater,
                                             SpeechUIModel* speech_ui)
    : model_updater_(model_updater),
      speech_ui_(speech_ui),
      is_fullscreen_app_list_enabled_(features::IsFullscreenAppListEnabled()) {
  speech_ui_->AddObserver(this);
  // Give |SearchBoxModel| tablet and clamshell A11y Announcements.
  model_updater_->SetSearchTabletAndClamshellAccessibleName(
      l10n_util::GetStringUTF16(IDS_SEARCH_BOX_ACCESSIBILITY_NAME_TABLET),
      l10n_util::GetStringUTF16(IDS_SEARCH_BOX_ACCESSIBILITY_NAME));
  OnSpeechRecognitionStateChanged(speech_ui_->state());
}

SearchResourceManager::~SearchResourceManager() {
  speech_ui_->RemoveObserver(this);
}

void SearchResourceManager::OnSpeechRecognitionStateChanged(
    SpeechRecognitionState new_state) {
  if (is_fullscreen_app_list_enabled_) {
    model_updater_->SetSearchHintText(
        l10n_util::GetStringUTF16(IDS_SEARCH_BOX_HINT_FULLSCREEN));
  } else {
    model_updater_->SetSearchHintText(
        l10n_util::GetStringUTF16(IDS_SEARCH_BOX_HINT));
    model_updater_->SetSearchSpeechRecognitionButton(new_state);
  }
}

}  // namespace app_list
