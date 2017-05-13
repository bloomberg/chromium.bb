// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_INIT_AWARE_MODEL_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_INIT_AWARE_MODEL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/feature_engagement_tracker/internal/model.h"

namespace feature_engagement_tracker {

class InitAwareModel : public Model {
 public:
  InitAwareModel(std::unique_ptr<Model> model);
  ~InitAwareModel() override;

  // Model implementation.
  void Initialize(const OnModelInitializationFinished& callback,
                  uint32_t current_day) override;
  bool IsReady() const override;
  const Event* GetEvent(const std::string& event_name) const override;
  void IncrementEvent(const std::string& event_name,
                      uint32_t current_day) override;

 private:
  void OnInitializeComplete(const OnModelInitializationFinished& callback,
                            bool success);

  std::unique_ptr<Model> model_;
  std::vector<std::tuple<std::string, uint32_t>> queued_events_;

  base::WeakPtrFactory<InitAwareModel> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(InitAwareModel);
};

}  // namespace feature_engagement_tracker

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_INIT_AWARE_MODEL_H_
