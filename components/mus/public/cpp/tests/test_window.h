// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_TESTS_TEST_WINDOW_H_
#define COMPONENTS_MUS_PUBLIC_CPP_TESTS_TEST_WINDOW_H_

#include "base/macros.h"
#include "components/mus/public/cpp/lib/window_private.h"
#include "components/mus/public/cpp/window.h"

namespace mus {

// Subclass with public ctor/dtor.
class TestWindow : public Window {
 public:
  TestWindow() { WindowPrivate(this).set_id(1); }
  explicit TestWindow(Id window_id) { WindowPrivate(this).set_id(window_id); }
  ~TestWindow() {}

 private:
  MOJO_DISALLOW_COPY_AND_ASSIGN(TestWindow);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_TESTS_TEST_WINDOW_H_
