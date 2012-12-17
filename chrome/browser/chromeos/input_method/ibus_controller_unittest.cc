// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/input_method/ibus_controller.h"
#include "chrome/browser/chromeos/input_method/mock_sequenced_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace input_method {

TEST(IBusControllerTest, TestCreate) {
  scoped_refptr<MockSequencedTaskRunner> default_task_runner(
      new MockSequencedTaskRunner);
  scoped_refptr<MockSequencedTaskRunner> worker_task_runner(
      new MockSequencedTaskRunner);
  scoped_ptr<IBusController> controller(IBusController::Create(
      default_task_runner, worker_task_runner));
  EXPECT_TRUE(controller.get());
}

}  // namespace input_method
}  // namespace chromeos
