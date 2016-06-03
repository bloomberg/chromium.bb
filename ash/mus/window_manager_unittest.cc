// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <utility>

#include "ash/public/interfaces/user_window_controller.mojom.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_client.h"
#include "components/mus/public/cpp/window_tree_client_delegate.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "services/shell/public/cpp/shell_test.h"

namespace ash {
namespace mus {

class WindowTreeClientDelegate : public ::mus::WindowTreeClientDelegate {
 public:
  WindowTreeClientDelegate() {}
  ~WindowTreeClientDelegate() override {}

 private:
  // mus::WindowTreeClientDelegate:
  void OnEmbed(::mus::Window* root) override {}
  void OnWindowTreeClientDestroyed(::mus::WindowTreeClient* client) override {}
  void OnEventObserved(const ui::Event& event, ::mus::Window* target) override {
  }

  DISALLOW_COPY_AND_ASSIGN(WindowTreeClientDelegate);
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
    connector->ConnectToInterface("mojo:ash", &user_window_controller_);
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
  WindowTreeClientDelegate window_tree_delegate;

  connector()->Connect("mojo:ash");

  // Connect to mus and create a new top level window. The request goes to
  // |ash|, but is async.
  std::unique_ptr<::mus::WindowTreeClient> client(
      new ::mus::WindowTreeClient(&window_tree_delegate, nullptr, nullptr));
  client->ConnectViaWindowTreeFactory(connector());
  ::mus::Window* top_level_window = client->NewTopLevelWindow(nullptr);
  ASSERT_TRUE(top_level_window);
  ::mus::Window* child_window = client->NewWindow();
  ASSERT_TRUE(child_window);
  top_level_window->AddChild(child_window);

  // Create another WindowTreeClient by way of embedding in
  // |child_window|. This blocks until it succeeds.
  ::mus::mojom::WindowTreeClientPtr tree_client;
  auto tree_client_request = GetProxy(&tree_client);
  child_window->Embed(std::move(tree_client), base::Bind(&OnEmbed));
  std::unique_ptr<::mus::WindowTreeClient> child_client(
      new ::mus::WindowTreeClient(&window_tree_delegate, nullptr,
                                  std::move(tree_client_request)));
  child_client->WaitForEmbed();
  ASSERT_TRUE(!child_client->GetRoots().empty());
}

TEST_F(WindowManagerTest, OpenWindowAndClose) {
  connector()->Connect("mojo:ash");

  TestUserWindowObserver observer(connector());

  // Connect to mus and create a new top level window.
  WindowTreeClientDelegate window_tree_delegate;
  std::unique_ptr<::mus::WindowTreeClient> client(
      new ::mus::WindowTreeClient(&window_tree_delegate, nullptr, nullptr));
  client->ConnectViaWindowTreeFactory(connector());
  ::mus::Window* top_level_window = client->NewTopLevelWindow(nullptr);
  ASSERT_TRUE(top_level_window);

  observer.WaitUntilWindowCountReaches(1u);
  client.reset();
  observer.WaitUntilWindowCountReaches(0u);
}

}  // namespace mus
}  // namespace ash
