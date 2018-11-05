// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_COMMON_LEARNING_SESSION_H_
#define MEDIA_LEARNING_COMMON_LEARNING_SESSION_H_

#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "media/learning/common/learning_task.h"

namespace media {
namespace learning {

// Interface to provide a Learner given the task name.
class COMPONENT_EXPORT(LEARNING_COMMON) LearningSession {
 public:
  LearningSession();
  virtual ~LearningSession();

  // Add an observed example of |instance| with target value |target| to the
  // learning task |task_name|.
  // TODO(liberato): Consider making this an enum to match mojo.
  virtual void AddExample(const std::string& task_name,
                          const Instance& instance,
                          const TargetValue& target) = 0;

  // TODO(liberato): Add prediction API.

 private:
  DISALLOW_COPY_AND_ASSIGN(LearningSession);
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_COMMON_LEARNING_SESSION_H_
