// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLES2_COMMAND_BUFFER_IMPL_OBSERVER_H_
#define COMPONENTS_GLES2_COMMAND_BUFFER_IMPL_OBSERVER_H_

namespace mus {

class CommandBufferImplObserver {
 public:
  virtual void OnCommandBufferImplDestroyed() = 0;

 protected:
  ~CommandBufferImplObserver() {}
};

}  // namespace mus

#endif  // COMPONENTS_GLES2_COMMAND_BUFFER_IMPL_OBSERVER_H_
