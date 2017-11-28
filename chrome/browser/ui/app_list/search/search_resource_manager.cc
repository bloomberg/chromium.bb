// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_resource_manager.h"

#include <memory>

#include "ash/app_list/model/search_box_model.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/app_list/start_page_service.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/speech_ui_model.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace app_list {

namespace {

std::unique_ptr<SearchBoxModel::SpeechButtonProperty> CreateNewProperty(
    SpeechRecognitionState state) {
  // Currently no speech support in app list.
  // TODO(xiaohuic): when implementing speech support in new app list, we should
  // either reuse this and related logic or delete them.
  return nullptr;
}

}  // namespace

SearchResourceManager::SearchResourceManager(Profile* profile,
                                             SearchBoxModel* search_box,
                                             SpeechUIModel* speech_ui)
    : search_box_(search_box),
      speech_ui_(speech_ui),
      is_fullscreen_app_list_enabled_(features::IsFullscreenAppListEnabled()) {
  speech_ui_->AddObserver(this);
  // Give |SearchBoxModel| tablet and clamshell A11y Announcements.
  search_box_->SetTabletAndClamshellAccessibleName(
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
    search_box_->SetHintText(
        l10n_util::GetStringUTF16(IDS_SEARCH_BOX_HINT_FULLSCREEN));
  } else {
    search_box_->SetHintText(l10n_util::GetStringUTF16(IDS_SEARCH_BOX_HINT));
    search_box_->SetSpeechRecognitionButton(CreateNewProperty(new_state));
  }
}

}  // namespace app_list
