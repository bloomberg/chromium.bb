// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/init_aware_model.h"

#include "base/bind.h"

namespace feature_engagement_tracker {

InitAwareModel::InitAwareModel(std::unique_ptr<Model> model)
    : model_(std::move(model)), weak_ptr_factory_(this) {
  DCHECK(model_);
}

InitAwareModel::~InitAwareModel() = default;

void InitAwareModel::Initialize(const OnModelInitializationFinished& callback,
                                uint32_t current_day) {
  model_->Initialize(base::Bind(&InitAwareModel::OnInitializeComplete,
                                weak_ptr_factory_.GetWeakPtr(), callback),
                     current_day);
}

bool InitAwareModel::IsReady() const {
  return model_->IsReady();
}

const Event* InitAwareModel::GetEvent(const std::string& event_name) const {
  return model_->GetEvent(event_name);
}

void InitAwareModel::IncrementEvent(const std::string& event_name,
                                    uint32_t current_day) {
  if (IsReady()) {
    model_->IncrementEvent(event_name, current_day);
    return;
  }

  queued_events_.push_back(std::tie(event_name, current_day));
}

void InitAwareModel::OnInitializeComplete(
    const OnModelInitializationFinished& callback,
    bool success) {
  if (success) {
    for (auto& event : queued_events_)
      model_->IncrementEvent(std::get<0>(event), std::get<1>(event));
    queued_events_.clear();
  }

  callback.Run(success);
}

}  // namespace feature_engagement_tracker
