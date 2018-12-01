// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_IMPL_LEARNING_TASK_CONTROLLER_IMPL_H_
#define MEDIA_LEARNING_IMPL_LEARNING_TASK_CONTROLLER_IMPL_H_

#include <memory>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "media/learning/impl/learning_task_controller.h"
#include "media/learning/impl/training_algorithm.h"

namespace media {
namespace learning {

class LearningTaskControllerImplTest;

class COMPONENT_EXPORT(LEARNING_IMPL) LearningTaskControllerImpl
    : public LearningTaskController,
      public base::SupportsWeakPtr<LearningTaskControllerImpl> {
 public:
  explicit LearningTaskControllerImpl(const LearningTask& task);
  ~LearningTaskControllerImpl() override;

  // LearningTaskController
  void AddExample(const TrainingExample& example) override;

 private:
  // Called with accuracy results as new examples are added.  Only tests should
  // need to worry about this.
  using AccuracyReportingCB =
      base::RepeatingCallback<void(const LearningTask& task, bool is_correct)>;

  // Override the training CB for testing.
  void SetTrainingCBForTesting(TrainingAlgorithmCB cb);

  // Override the reporting CB for testing.
  void SetAccuracyReportingCBForTesting(AccuracyReportingCB cb);

  // Called by |training_cb_| when the model is trained.
  void OnModelTrained(std::unique_ptr<Model> model);

  LearningTask task_;

  // Current batch of examples.
  scoped_refptr<TrainingDataStorage> storage_;

  // Most recently trained model, or null.
  std::unique_ptr<Model> model_;

  TrainingAlgorithmCB training_cb_;

  AccuracyReportingCB accuracy_reporting_cb_;

  friend class LearningTaskControllerImplTest;
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_IMPL_LEARNING_TASK_CONTROLLER_IMPL_H_
