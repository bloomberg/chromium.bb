// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/model_impl.h"

#include <map>
#include <memory>

#include "components/feature_engagement_tracker/internal/configuration.h"
#include "components/feature_engagement_tracker/internal/model.h"
#include "components/feature_engagement_tracker/internal/store.h"

namespace feature_engagement_tracker {

ModelImpl::ModelImpl(std::unique_ptr<Store> store,
                     std::unique_ptr<Configuration> configuration)
    : Model(),
      store_(std::move(store)),
      configuration_(std::move(configuration)),
      ready_(false),
      currently_showing_(false) {}

ModelImpl::~ModelImpl() = default;

void ModelImpl::Initialize(const OnModelInitializationFinished& callback) {
  // TODO(nyquist): Initialize Store and post result back to callback.
  // Only set ready when Store has fully loaded.
  ready_ = true;
}

bool ModelImpl::IsReady() const {
  return ready_;
}

const FeatureConfig& ModelImpl::GetFeatureConfig(
    const base::Feature& feature) const {
  return configuration_->GetFeatureConfig(feature);
}

void ModelImpl::SetIsCurrentlyShowing(bool is_showing) {
  currently_showing_ = is_showing;
}

bool ModelImpl::IsCurrentlyShowing() const {
  return currently_showing_;
}

}  // namespace feature_engagement_tracker
