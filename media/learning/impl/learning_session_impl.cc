// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/learning_session_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "media/learning/impl/distribution_reporter.h"
#include "media/learning/impl/learning_task_controller_impl.h"

namespace media {
namespace learning {

LearningSessionImpl::LearningSessionImpl(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)),
      controller_factory_(base::BindRepeating(
          [](scoped_refptr<base::SequencedTaskRunner> task_runner,
             const LearningTask& task,
             SequenceBoundFeatureProvider feature_provider)
              -> base::SequenceBound<LearningTaskController> {
            return base::SequenceBound<LearningTaskControllerImpl>(
                task_runner, task, DistributionReporter::Create(task),
                std::move(feature_provider));
          })) {}

LearningSessionImpl::~LearningSessionImpl() = default;

void LearningSessionImpl::SetTaskControllerFactoryCBForTesting(
    CreateTaskControllerCB cb) {
  controller_factory_ = std::move(cb);
}

void LearningSessionImpl::AddExample(const std::string& task_name,
                                     const LabelledExample& example) {
  auto iter = task_map_.find(task_name);
  if (iter != task_map_.end()) {
    // TODO(liberato): We shouldn't be adding examples.  We should provide the
    // LearningTaskController instead, although ownership gets a bit weird.
    LearningTaskController::ObservationId id = 1;
    iter->second.Post(FROM_HERE, &LearningTaskController::BeginObservation, id,
                      example.features);
    iter->second.Post(
        FROM_HERE, &LearningTaskController::CompleteObservation, id,
        ObservationCompletion(example.target_value, example.weight));
  }
}

void LearningSessionImpl::RegisterTask(
    const LearningTask& task,
    SequenceBoundFeatureProvider feature_provider) {
  DCHECK(task_map_.count(task.name) == 0);
  task_map_.emplace(
      task.name,
      controller_factory_.Run(task_runner_, task, std::move(feature_provider)));
}

}  // namespace learning
}  // namespace media
