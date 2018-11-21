// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_MOJO_MOJO_LEARNING_SESSION_IMPL_H_
#define MEDIA_LEARNING_MOJO_MOJO_LEARNING_SESSION_IMPL_H_

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "media/learning/mojo/public/mojom/learning_session.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace media {
namespace learning {

class LearningSession;
class MojoLearningSessionImplTest;

// Mojo service that talks to a local LearningSession.
class COMPONENT_EXPORT(MEDIA_LEARNING_MOJO) MojoLearningSessionImpl
    : public mojom::LearningSession {
 public:
  ~MojoLearningSessionImpl() override;

  // Bind |request| to this instance.
  void Bind(mojom::LearningSessionRequest request);

  // mojom::LearningSession
  void AddExample(mojom::LearningTaskType task_type,
                  const TrainingExample& example) override;

 protected:
  explicit MojoLearningSessionImpl(
      std::unique_ptr<::media::learning::LearningSession> impl);

  // Underlying session to which we proxy calls.
  std::unique_ptr<::media::learning::LearningSession> impl_;

  // We own our own bindings.  If we're ever not a singleton, then this should
  // move to our owner.
  mojo::BindingSet<mojom::LearningSession> self_bindings_;

  friend class MojoLearningSessionImplTest;

  DISALLOW_COPY_AND_ASSIGN(MojoLearningSessionImpl);
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_MOJO_MOJO_LEARNING_SESSION_IMPL_H_
