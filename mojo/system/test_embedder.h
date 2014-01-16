// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_TEST_EMBEDDER_H_
#define MOJO_SYSTEM_TEST_EMBEDDER_H_

namespace mojo {
namespace embedder {
namespace test {

// This shuts down the global, singleton instance. (Note: "Real" embedders are
// not expected to ever shut down this instance. This |Shutdown()| function will
// do more work to ensure that tests don't leak, etc.)
// TODO(vtl): Figure out the library/component/DLL/export situation for test
// embedder stuff. For now, it's linked directly into the unit test binary.
void Shutdown();

}  // namespace test
}  // namespace embedder
}  // namespace mojo

#endif  // MOJO_SYSTEM_EMBEDDER_H_
