// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "mojo/shell/public/cpp/application_test_base.h"
#include "mojo/shell/public/interfaces/application_manager.mojom.h"
#include "mojo/shell/tests/connect/connect_test.mojom.h"

// Tests that multiple applications can be packaged in a single Mojo application
// implementing ShellClientFactory; that these applications can be specified by
// the package's manifest and are thus registered with the PackageManager.

namespace mojo {
namespace shell {
namespace {
void ReceiveName(std::string* out_name,
                 base::RunLoop* loop,
                 const String& name) {
  *out_name = name;
  loop->Quit();
}
}  // namespace

using ConnectApptest = mojo::test::ApplicationTestBase;

TEST_F(ConnectApptest, Basic) {
  std::set<uint32_t> ids;
  {
    // We need to do this to force the shell to read the test app's manifest and
    // register aliases.
    test::mojom::ConnectTestServicePtr root_service;
    scoped_ptr<Connection> connection =
        connector()->Connect("mojo:connect_test_package");
    connection->GetInterface(&root_service);
    base::RunLoop run_loop;
    std::string root_name;
    root_service->GetName(base::Bind(&ReceiveName, &root_name, &run_loop));
    run_loop.Run();
    uint32_t id = mojom::Connector::kInvalidApplicationID;
    EXPECT_TRUE(connection->GetRemoteApplicationID(&id));
    ids.insert(id);
  }

  {
    // Now subsequent connects to applications provided by the root app will be
    // resolved correctly.
    test::mojom::ConnectTestServicePtr service_a;
    scoped_ptr<Connection> connection =
        connector()->Connect("mojo:connect_test_a");
    connection->GetInterface(&service_a);
    base::RunLoop run_loop;
    std::string a_name;
    service_a->GetName(base::Bind(&ReceiveName, &a_name, &run_loop));
    run_loop.Run();
    EXPECT_EQ("A", a_name);
    uint32_t id = mojom::Connector::kInvalidApplicationID;
    EXPECT_TRUE(connection->GetRemoteApplicationID(&id));
    ids.insert(id);
  }

  {
    test::mojom::ConnectTestServicePtr service_b;
    scoped_ptr<Connection> connection =
        connector()->Connect("mojo:connect_test_b");
    connection->GetInterface(&service_b);
    base::RunLoop run_loop;
    std::string b_name;
    service_b->GetName(base::Bind(&ReceiveName, &b_name, &run_loop));
    run_loop.Run();
    EXPECT_EQ("B", b_name);
    uint32_t id = mojom::Connector::kInvalidApplicationID;
    EXPECT_TRUE(connection->GetRemoteApplicationID(&id));
    ids.insert(id);
  }
}

}  // namespace shell
}  // namespace mojo
