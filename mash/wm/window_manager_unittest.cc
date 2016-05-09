// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/public/cpp/window_tree_delegate.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "mash/wm/public/interfaces/user_window_controller.mojom.h"
#include "services/shell/public/cpp/shell_test.h"

namespace mash {
namespace wm {

class WindowTreeDelegateImpl : public mus::WindowTreeDelegate {
 public:
  WindowTreeDelegateImpl() {}
  ~WindowTreeDelegateImpl() override {}

 private:
  // mus::WindowTreeDelegate:
  void OnEmbed(mus::Window* root) override {}
  void OnConnectionLost(mus::WindowTreeConnection* connection) override {}
  void OnEventObserved(const ui::Event& event, mus::Window* target) override {}

  DISALLOW_COPY_AND_ASSIGN(WindowTreeDelegateImpl);
};

class WindowManagerTest : public shell::test::ShellTest {
 public:
  WindowManagerTest() : shell::test::ShellTest("exe:mash_unittests") {}
  ~WindowManagerTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowManagerTest);
};

void OnEmbed(bool success) {
  ASSERT_TRUE(success);
}

class TestUserWindowObserver : public mojom::UserWindowObserver {
 public:
  explicit TestUserWindowObserver(shell::Connector* connector)
      : binding_(this), window_count_(0u), expected_window_count_(0u) {
    connector->ConnectToInterface("mojo:desktop_wm", &user_window_controller_);
    user_window_controller_->AddUserWindowObserver(
        binding_.CreateInterfacePtrAndBind());
  }

  ~TestUserWindowObserver() override {}

  bool WaitUntilWindowCountReaches(size_t expected) {
    DCHECK(quit_callback_.is_null());
    if (window_count_ != expected) {
      base::RunLoop loop;
      quit_callback_ = loop.QuitClosure();
      expected_window_count_ = expected;
      loop.Run();
      quit_callback_ = base::Closure();
    }
    return window_count_ == expected;
  }

 private:
  void QuitIfNecessary() {
    if (window_count_ == expected_window_count_ && !quit_callback_.is_null())
      quit_callback_.Run();
  }

  // mojom::UserWindowObserver:
  void OnUserWindowObserverAdded(
      mojo::Array<mojom::UserWindowPtr> user_windows) override {
    window_count_ = user_windows.size();
    QuitIfNecessary();
  }

  void OnUserWindowAdded(mojom::UserWindowPtr user_window) override {
    ++window_count_;
    QuitIfNecessary();
  }

  void OnUserWindowRemoved(uint32_t window_id) override {
    ASSERT_TRUE(window_count_);
    --window_count_;
    QuitIfNecessary();
  }

  void OnUserWindowTitleChanged(uint32_t window_id,
                                const mojo::String& window_title) override {}
  void OnUserWindowFocusChanged(uint32_t window_id, bool has_focus) override {}
  void OnUserWindowAppIconChanged(uint32_t window_id,
                                  mojo::Array<uint8_t> app_icon) override {}

  mojom::UserWindowControllerPtr user_window_controller_;
  mojo::Binding<mojom::UserWindowObserver> binding_;

  size_t window_count_;
  size_t expected_window_count_;
  base::Closure quit_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestUserWindowObserver);
};

TEST_F(WindowManagerTest, OpenWindow) {
  WindowTreeDelegateImpl window_tree_delegate;

  // Bring up the the desktop_wm.
  connector()->Connect("mojo:desktop_wm");

  // Connect to mus and create a new top level window. The request goes to
  // the |desktop_wm|, but is async.
  std::unique_ptr<mus::WindowTreeConnection> connection(
      mus::WindowTreeConnection::Create(&window_tree_delegate, connector()));
  mus::Window* top_level_window = connection->NewTopLevelWindow(nullptr);
  ASSERT_TRUE(top_level_window);
  mus::Window* child_window = connection->NewWindow();
  ASSERT_TRUE(child_window);
  top_level_window->AddChild(child_window);

  // Create another WindowTreeConnection by way of embedding in
  // |child_window|. This blocks until it succeeds.
  mus::mojom::WindowTreeClientPtr tree_client;
  auto tree_client_request = GetProxy(&tree_client);
  child_window->Embed(std::move(tree_client), base::Bind(&OnEmbed));
  std::unique_ptr<mus::WindowTreeConnection> child_connection(
      mus::WindowTreeConnection::Create(
          &window_tree_delegate, std::move(tree_client_request),
          mus::WindowTreeConnection::CreateType::WAIT_FOR_EMBED));
  ASSERT_TRUE(!child_connection->GetRoots().empty());
}

TEST_F(WindowManagerTest, OpenWindowAndClose) {
  // Bring up the the desktop_wm.
  connector()->Connect("mojo:desktop_wm");

  TestUserWindowObserver observer(connector());

  // Connect to mus and create a new top level window.
  WindowTreeDelegateImpl window_tree_delegate;
  std::unique_ptr<mus::WindowTreeConnection> connection(
      mus::WindowTreeConnection::Create(&window_tree_delegate, connector()));
  mus::Window* top_level_window = connection->NewTopLevelWindow(nullptr);
  ASSERT_TRUE(top_level_window);

  observer.WaitUntilWindowCountReaches(1u);
  connection.reset();
  observer.WaitUntilWindowCountReaches(0u);
}

}  // namespace wm
}  // namespace mash
