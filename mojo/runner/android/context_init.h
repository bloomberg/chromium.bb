// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_RUNNER_ANDROID_CONTEXT_INIT_H_
#define MOJO_RUNNER_ANDROID_CONTEXT_INIT_H_

namespace mojo {
namespace runner {

class Context;

// Any Context specific initialization goes here.
void InitContext(Context* context);

}  // namespace runner
}  // namespace mojo

#endif  // MOJO_RUNNER_ANDROID_CONTEXT_INIT_H_
