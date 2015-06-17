// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_timeouts.h"
#include "components/view_manager/public/cpp/tests/view_manager_test_base.h"
#include "components/view_manager/public/cpp/view.h"
#include "components/view_manager/public/cpp/view_manager.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/application_test_base.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/mojo_services/src/accessibility/public/interfaces/accessibility.mojom.h"

namespace mojo {

namespace {

// Returns true if the tree contains a text node with contents matching |text|.
bool AxTreeContainsText(const Array<AxNodePtr>& tree, const String& text) {
  for (size_t i = 0; i < tree.size(); ++i) {
    if (!tree[i]->text.is_null() && tree[i]->text->content == text)
      return true;
  }
  return false;
}

}  // namespace

using AXProviderTest = ViewManagerTestBase;

TEST_F(AXProviderTest, HelloWorld) {
  // Start a test server for net/data/test.html access.
  net::SpawnedTestServer server(
      net::SpawnedTestServer::TYPE_HTTP, net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("net/data")));
  ASSERT_TRUE(server.Start());

  // Connect to the URL through the mojo:html_viewer content handler.
  const uint16_t assigned_port = server.host_port_pair().port();
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From(
      base::StringPrintf("http://127.0.0.1:%u/files/test.html", assigned_port));
  ApplicationConnection* connection = application_impl()->ConnectToApplication(
      request.Pass());

  // Embed the html_viewer in a View.
  ViewManagerClientPtr view_manager_client;
  connection->ConnectToService(&view_manager_client);
  View* embed_view = window_manager()->CreateView();
  embed_view->Embed(view_manager_client.Pass());

  // Connect to the AxProvider of the HTML document and get the AxTree.
  AxProviderPtr ax_provider;
  connection->ConnectToService(&ax_provider);
  Array<AxNodePtr> ax_tree;
  ax_provider->GetTree([&ax_tree](Array<AxNodePtr> tree) {
                         ax_tree = tree.Pass();
                         EXPECT_TRUE(QuitRunLoop());
                       });
  ASSERT_TRUE(DoRunLoopWithTimeout());

  EXPECT_TRUE(AxTreeContainsText(ax_tree, "Hello "));
  EXPECT_TRUE(AxTreeContainsText(ax_tree, "World!"));
  EXPECT_FALSE(AxTreeContainsText(ax_tree, "foo"));
}

}  // namespace mojo
