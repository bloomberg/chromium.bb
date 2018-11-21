// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/mojo/public/cpp/mojo_learning_session.h"

#include "mojo/public/cpp/bindings/binding.h"

namespace media {
namespace learning {

MojoLearningSession::MojoLearningSession(mojom::LearningSessionPtr session_ptr)
    : session_ptr_(std::move(session_ptr)) {}

MojoLearningSession::~MojoLearningSession() = default;

void MojoLearningSession::AddExample(const std::string& task_name,
                                     const TrainingExample& example) {
  // TODO(liberato): Convert from |task_name| to a task type.
  session_ptr_->AddExample(mojom::LearningTaskType::kPlaceHolderTask, example);
}

}  // namespace learning
}  // namespace media
