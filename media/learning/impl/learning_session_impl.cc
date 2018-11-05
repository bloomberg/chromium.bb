// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/learning_session_impl.h"

#include "base/logging.h"

namespace media {
namespace learning {

LearningSessionImpl::LearningSessionImpl() = default;
LearningSessionImpl::~LearningSessionImpl() = default;

void LearningSessionImpl::AddExample(const std::string& task_name,
                                     const Instance& instance,
                                     const TargetValue& target) {
  // TODO: match |task_name| against a list of learning tasks, and find the
  // learner(s) for it.  Then add |instance|, |target| to it.
  NOTIMPLEMENTED();
}

}  // namespace learning
}  // namespace media
