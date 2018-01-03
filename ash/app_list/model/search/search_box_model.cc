// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/model/search/search_box_model.h"

#include <utility>

#include "ash/app_list/model/search/search_box_model_observer.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"

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

SearchBoxModel::SpeechButtonProperty::SpeechButtonProperty(
    const gfx::ImageSkia& icon,
    const base::string16& tooltip,
    const base::string16& accessible_name)
    : icon(icon), tooltip(tooltip), accessible_name(accessible_name) {}

SearchBoxModel::SpeechButtonProperty::~SpeechButtonProperty() = default;

SearchBoxModel::SearchBoxModel() : is_tablet_mode_(false) {}

SearchBoxModel::~SearchBoxModel() = default;

void SearchBoxModel::SetSpeechRecognitionButton(SpeechRecognitionState state) {
  speech_button_ = CreateNewProperty(state);
  for (auto& observer : observers_)
    observer.SpeechRecognitionButtonPropChanged();
}

void SearchBoxModel::SetHintText(const base::string16& hint_text) {
  if (hint_text_ == hint_text)
    return;

  hint_text_ = hint_text;
  for (auto& observer : observers_)
    observer.HintTextChanged();
}

void SearchBoxModel::SetTabletAndClamshellAccessibleName(
    base::string16 tablet_accessible_name,
    base::string16 clamshell_accessible_name) {
  tablet_accessible_name_ = tablet_accessible_name;
  clamshell_accessible_name_ = clamshell_accessible_name;
  UpdateAccessibleName();
}

void SearchBoxModel::UpdateAccessibleName() {
  accessible_name_ =
      is_tablet_mode_ ? tablet_accessible_name_ : clamshell_accessible_name_;

  for (auto& observer : observers_)
    observer.HintTextChanged();
}

void SearchBoxModel::SetSelectionModel(const gfx::SelectionModel& sel) {
  if (selection_model_ == sel)
    return;

  selection_model_ = sel;
  for (auto& observer : observers_)
    observer.SelectionModelChanged();
}

void SearchBoxModel::SetTabletMode(bool started) {
  if (started == is_tablet_mode_)
    return;
  is_tablet_mode_ = started;
  UpdateAccessibleName();
}

void SearchBoxModel::Update(const base::string16& text,
                            bool initiated_by_user) {
  if (text_ == text)
    return;

  if (initiated_by_user) {
    if (text_.empty() && !text.empty()) {
      UMA_HISTOGRAM_ENUMERATION("Apps.AppListSearchCommenced", 1, 2);
      base::RecordAction(base::UserMetricsAction("AppList_EnterSearch"));
    } else if (!text_.empty() && text.empty()) {
      base::RecordAction(base::UserMetricsAction("AppList_LeaveSearch"));
    }
  }
  text_ = text;
  for (auto& observer : observers_)
    observer.Update();
}

void SearchBoxModel::AddObserver(SearchBoxModelObserver* observer) {
  observers_.AddObserver(observer);
}

void SearchBoxModel::RemoveObserver(SearchBoxModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace app_list
