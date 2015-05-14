// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_timeouts.h"
#include "mojo/application/application_test_base_chromium.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/mojo_services/src/accessibility/public/interfaces/accessibility.mojom.h"

namespace mojo {

namespace {

base::RunLoop* current_run_loop = nullptr;

void TimeoutRunLoop(const base::Closure& timeout_task, bool* timeout) {
  CHECK(current_run_loop);
  *timeout = true;
  timeout_task.Run();
}

bool DoRunLoopWithTimeout() {
  if (current_run_loop != nullptr)
    return false;

  bool timeout = false;
  base::RunLoop run_loop;
  base::MessageLoop::current()->PostDelayedTask(
    FROM_HERE, base::Bind(&TimeoutRunLoop, run_loop.QuitClosure(), &timeout),
    TestTimeouts::action_timeout());

  current_run_loop = &run_loop;
  current_run_loop->Run();
  current_run_loop = nullptr;
  return !timeout;
}

void QuitRunLoop() {
  current_run_loop->Quit();
  current_run_loop = nullptr;
}

// Returns true if the tree contains a text node with contents matching |text|.
bool AxTreeContainsText(const Array<AxNodePtr>& tree, const String& text) {
  for (size_t i = 0; i < tree.size(); ++i) {
    if (!tree[i]->text.is_null() && tree[i]->text->content == text)
      return true;
  }
  return false;
}

}  // namespace

typedef test::ApplicationTestBase AXProviderTest;

TEST_F(AXProviderTest, HelloWorld) {
  // Start a test server for net/data/test.html access.
  net::SpawnedTestServer server(
      net::SpawnedTestServer::TYPE_HTTP, net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("net/data")));
  ASSERT_TRUE(server.Start());

  // Connect to the URL through the mojo:html_viewer content handler.
  const uint16_t assigned_port = server.host_port_pair().port();
  ApplicationConnection* connection = application_impl()->ConnectToApplication(
      base::StringPrintf("http://127.0.0.1:%u/files/test.html", assigned_port));

  // Connect to the AxProvider of the HTML document and get the AxTree.
  AxProviderPtr ax_provider;
  connection->ConnectToService(&ax_provider);
  Array<AxNodePtr> ax_tree;
  ax_provider->GetTree([&ax_tree](Array<AxNodePtr> tree) {
                         ax_tree = tree.Pass();
                         QuitRunLoop();
                       });
  ASSERT_TRUE(DoRunLoopWithTimeout());

  EXPECT_TRUE(AxTreeContainsText(ax_tree, "Hello "));
  EXPECT_TRUE(AxTreeContainsText(ax_tree, "World!"));
  EXPECT_FALSE(AxTreeContainsText(ax_tree, "foo"));
}

}  // namespace mojo