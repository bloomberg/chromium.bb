// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ime_controller_client.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_manager_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

using ImeControllerClientTest = testing::Test;

TEST_F(ImeControllerClientTest, Basics) {
  chromeos::input_method::MockInputMethodManagerImpl input_method_manager;

  std::unique_ptr<ImeControllerClient> client =
      base::MakeUnique<ImeControllerClient>(&input_method_manager);
  EXPECT_EQ(1, input_method_manager.add_observer_count());
  EXPECT_EQ(1, input_method_manager.add_menu_observer_count());

  client.reset();
  EXPECT_EQ(1, input_method_manager.remove_observer_count());
  EXPECT_EQ(1, input_method_manager.remove_menu_observer_count());
}

// TODO(jamescook): When ImeControllerClient switches to using mojo add
// additional tests that the correct mojo interface methods are called to send
// data to ash.
