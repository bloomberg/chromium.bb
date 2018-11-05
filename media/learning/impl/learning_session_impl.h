// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_IMPL_LEARNING_SESSION_IMPL_H_
#define MEDIA_LEARNING_IMPL_LEARNING_SESSION_IMPL_H_

#include "base/component_export.h"
#include "media/learning/common/learning_session.h"

namespace media {
namespace learning {

// Concrete implementation of a LearningSession.  This would have a list of
// learning tasks, and could provide local learners for each task.
class COMPONENT_EXPORT(LEARNING_IMPL) LearningSessionImpl
    : public LearningSession {
 public:
  LearningSessionImpl();
  ~LearningSessionImpl() override;

  // LearningSession
  void AddExample(const std::string& task_name,
                  const Instance& instance,
                  const TargetValue& target) override;
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_IMPL_LEARNING_SESSION_IMPL_H_
