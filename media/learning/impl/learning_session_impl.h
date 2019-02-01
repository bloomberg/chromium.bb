// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_IMPL_LEARNING_SESSION_IMPL_H_
#define MEDIA_LEARNING_IMPL_LEARNING_SESSION_IMPL_H_

#include <map>

#include "base/component_export.h"
#include "base/threading/sequence_bound.h"
#include "media/learning/common/learning_session.h"
#include "media/learning/impl/feature_provider.h"
#include "media/learning/impl/learning_task_controller.h"

namespace media {
namespace learning {

// Concrete implementation of a LearningSession.  This allows registration of
// learning tasks.
class COMPONENT_EXPORT(LEARNING_IMPL) LearningSessionImpl
    : public LearningSession {
 public:
  LearningSessionImpl();
  ~LearningSessionImpl() override;

  using CreateTaskControllerCB =
      base::RepeatingCallback<std::unique_ptr<LearningTaskController>(
          const LearningTask&,
          SequenceBoundFeatureProvider)>;

  void SetTaskControllerFactoryCBForTesting(CreateTaskControllerCB cb);

  // LearningSession
  void AddExample(const std::string& task_name,
                  const LabelledExample& example) override;

  // Registers |task|, so that calls to AddExample with |task.name| will work.
  // This will create a new controller for the task.
  void RegisterTask(const LearningTask& task,
                    SequenceBoundFeatureProvider feature_provider =
                        SequenceBoundFeatureProvider());

 private:
  // [task_name] = task controller.
  using LearningTaskMap =
      std::map<std::string, std::unique_ptr<LearningTaskController>>;
  LearningTaskMap task_map_;

  CreateTaskControllerCB controller_factory_;
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_IMPL_LEARNING_SESSION_IMPL_H_
