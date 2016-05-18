// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/tests/test_window_tree_connection_setup.h"

#include "components/mus/public/cpp/lib/window_tree_client_impl.h"
#include "components/mus/public/cpp/tests/test_window_tree.h"
#include "components/mus/public/cpp/tests/window_tree_client_impl_private.h"

namespace mus {

TestWindowTreeConnectionSetup::TestWindowTreeConnectionSetup() {}

TestWindowTreeConnectionSetup::~TestWindowTreeConnectionSetup() {
  std::unique_ptr<WindowTreeConnection> window_tree_client =
      std::move(window_tree_client_);
  if (window_tree_client)
    window_tree_client->RemoveObserver(this);
}

void TestWindowTreeConnectionSetup::Init(
    WindowTreeDelegate* window_tree_delegate,
    WindowManagerDelegate* window_manager_delegate) {
  window_tree_.reset(new TestWindowTree);
  window_tree_client_.reset(new WindowTreeClientImpl(
      window_tree_delegate, window_manager_delegate, nullptr));
  static_cast<WindowTreeConnection*>(window_tree_client_.get())
      ->AddObserver(this);
  WindowTreeClientImplPrivate(window_tree_client_.get())
      .OnEmbed(window_tree_.get());
}

WindowTreeConnection* TestWindowTreeConnectionSetup::window_tree_connection() {
  return window_tree_client_.get();
}

void TestWindowTreeConnectionSetup::OnWillDestroyConnection(
    mus::WindowTreeConnection* connection) {
  // See comment in header as to why we do this.
  window_tree_client_.release();
}

}  // namespace mus
