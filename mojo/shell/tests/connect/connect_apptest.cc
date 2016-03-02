// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/shell/public/cpp/application_test_base.h"
#include "mojo/shell/public/interfaces/application_manager.mojom.h"
#include "mojo/shell/tests/connect/connect_test.mojom.h"

// Tests that multiple applications can be packaged in a single Mojo application
// implementing ShellClientFactory; that these applications can be specified by
// the package's manifest and are thus registered with the PackageManager.

namespace mojo {
namespace shell {
namespace {
const char kTestPackageName[] = "mojo:connect_test_package";
const char kTestAppName[] = "mojo:connect_test_app";
const char kTestAppAName[] = "mojo:connect_test_a";
const char kTestAppBName[] = "mojo:connect_test_b";

void ReceiveTitle(std::string* out_name,
                 base::RunLoop* loop,
                 const String& name) {
  *out_name = name;
  loop->Quit();
}

void QuitLoop(base::RunLoop* loop) {
  loop->Quit();
}

}  // namespace

class ConnectApptest : public mojo::test::ApplicationTestBase,
                       public InterfaceFactory<test::mojom::ExposedInterface>,
                       public test::mojom::ExposedInterface {
 public:
  ConnectApptest() {}
  ~ConnectApptest() override {}

 protected:
  void set_ping_callback(const base::Closure& callback) {
    ping_callback_ = callback;
  }

  void CompareConnectionState(
      const std::string& connection_local_name,
      const std::string& connection_remote_name,
      uint32_t connection_remote_userid,
      uint32_t connection_remote_id,
      const std::string& initialize_local_name,
      uint32_t initialize_id,
      uint32_t initialize_userid) {
    EXPECT_EQ(connection_local_name, connection_state_->connection_local_name);
    EXPECT_EQ(connection_remote_name,
              connection_state_->connection_remote_name);
    EXPECT_EQ(connection_remote_userid,
              connection_state_->connection_remote_userid);
    EXPECT_EQ(connection_remote_id, connection_state_->connection_remote_id);
    EXPECT_EQ(initialize_local_name, connection_state_->initialize_local_name);
    EXPECT_EQ(initialize_id, connection_state_->initialize_id);
    EXPECT_EQ(initialize_userid, connection_state_->initialize_userid);
  }

 private:
  // mojo::test::ApplicationTestBase:
  void SetUp() override {
    mojo::test::ApplicationTestBase::SetUp();
    // We need to connect to the package first to force the shell to read the
    // package app's manifest and register aliases for the applications it
    // provides.
    test::mojom::ConnectTestServicePtr root_service;
    scoped_ptr<Connection> connection = connector()->Connect(kTestPackageName);
    connection->GetInterface(&root_service);
    base::RunLoop run_loop;
    std::string root_name;
    root_service->GetTitle(base::Bind(&ReceiveTitle, &root_name, &run_loop));
    run_loop.Run();
  }

  // InterfaceFactory<test::mojom::ExposedInterface>:
  void Create(Connection* connection,
              test::mojom::ExposedInterfaceRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }
  void ConnectionAccepted(test::mojom::ConnectionStatePtr state) override {
    connection_state_ = std::move(state);
    ping_callback_.Run();
  }

  base::Closure ping_callback_;
  test::mojom::ConnectionStatePtr connection_state_;

  BindingSet<test::mojom::ExposedInterface> bindings_;

  DISALLOW_COPY_AND_ASSIGN(ConnectApptest);
};

// Ensure the connection was properly established and that a round trip
// method call/response is completed.
TEST_F(ConnectApptest, Connect) {
  scoped_ptr<Connection> connection = connector()->Connect(kTestAppName);
  test::mojom::ConnectTestServicePtr service;
  connection->GetInterface(&service);
  base::RunLoop run_loop;
  std::string title;
  service->GetTitle(base::Bind(&ReceiveTitle, &title, &run_loop));
  run_loop.Run();
  EXPECT_EQ("APP", title);
  uint32_t id = mojom::Connector::kInvalidApplicationID;
  EXPECT_TRUE(connection->GetRemoteApplicationID(&id));
  EXPECT_NE(id, mojom::Connector::kInvalidApplicationID);
  EXPECT_EQ(connection->GetRemoteApplicationName(), kTestAppName);
}

// BlockedInterface should not be exposed to this application because it is not
// in our CapabilityFilter whitelist.
TEST_F(ConnectApptest, BlockedInterface) {
  scoped_ptr<Connection> connection = connector()->Connect(kTestAppName);
  base::RunLoop run_loop;
  test::mojom::BlockedInterfacePtr blocked;
  connection->GetInterface(&blocked);
  blocked.set_connection_error_handler(base::Bind(&QuitLoop, &run_loop));
  std::string title = "unchanged";
  blocked->GetTitleBlocked(base::Bind(&ReceiveTitle, &title, &run_loop));
  run_loop.Run();
  EXPECT_EQ("unchanged", title);
}

// Connects to an app provided by a package.
TEST_F(ConnectApptest, PackagedApp) {
  scoped_ptr<Connection> connection = connector()->Connect(kTestAppAName);
  test::mojom::ConnectTestServicePtr service_a;
  connection->GetInterface(&service_a);
  base::RunLoop run_loop;
  std::string a_name;
  service_a->GetTitle(base::Bind(&ReceiveTitle, &a_name, &run_loop));
  run_loop.Run();
  EXPECT_EQ("A", a_name);
  uint32_t id = mojom::Connector::kInvalidApplicationID;
  EXPECT_TRUE(connection->GetRemoteApplicationID(&id));
  EXPECT_NE(id, mojom::Connector::kInvalidApplicationID);
  EXPECT_EQ(connection->GetRemoteApplicationName(), kTestAppAName);
}

// Ask the target application to attempt to connect to a third application
// provided by a package whose id is permitted by the primary target's
// CapabilityFilter but whose package is not. The connection should be
// blocked and the returned title should be "uninitialized".
TEST_F(ConnectApptest, BlockedPackage) {
  scoped_ptr<Connection> connection = connector()->Connect(kTestAppName);
  test::mojom::StandaloneAppPtr standalone_app;
  connection->GetInterface(&standalone_app);
  base::RunLoop run_loop;
  std::string title;
  standalone_app->ConnectToAllowedAppInBlockedPackage(
      base::Bind(&ReceiveTitle, &title, &run_loop));
  run_loop.Run();
  EXPECT_EQ("uninitialized", title);
}

// BlockedInterface should not be exposed to this application because it is not
// in our CapabilityFilter whitelist.
TEST_F(ConnectApptest, PackagedApp_BlockedInterface) {
  scoped_ptr<Connection> connection = connector()->Connect(kTestAppAName);
  base::RunLoop run_loop;
  test::mojom::BlockedInterfacePtr blocked;
  connection->GetInterface(&blocked);
  blocked.set_connection_error_handler(base::Bind(&QuitLoop, &run_loop));
  run_loop.Run();
}

// Connection to another application provided by the same package, blocked
// because it's not in the capability filter whitelist.
TEST_F(ConnectApptest, BlockedPackagedApplication) {
  scoped_ptr<Connection> connection = connector()->Connect(kTestAppBName);
  test::mojom::ConnectTestServicePtr service_b;
  connection->GetInterface(&service_b);
  base::RunLoop run_loop;
  connection->SetRemoteInterfaceProviderConnectionErrorHandler(
      base::Bind(&QuitLoop, &run_loop));
  run_loop.Run();
  uint32_t id = mojom::Connector::kInvalidApplicationID;
  EXPECT_TRUE(connection->GetRemoteApplicationID(&id));
  EXPECT_EQ(id, mojom::Connector::kInvalidApplicationID);
}

// Tests that we can expose an interface to targets on outbound connections.
// TODO(beng): Currently all interfaces on outbound connections are exposed.
//             See ConnectorImpl::Connect().
TEST_F(ConnectApptest, LocalInterface) {
  // Connect to a standalone application.
  {
    test::mojom::ConnectTestServicePtr service;
    scoped_ptr<Connection> connection = connector()->Connect(kTestAppName);
    connection->GetInterface(&service);
    connection->AddInterface<test::mojom::ExposedInterface>(this);

    uint32_t remote_id = mojom::Connector::kInvalidApplicationID;
    {
      base::RunLoop run_loop;
      EXPECT_FALSE(connection->GetRemoteApplicationID(&remote_id));
      EXPECT_EQ(mojom::Connector::kInvalidApplicationID, remote_id);
      connection->AddRemoteIDCallback(base::Bind(&QuitLoop, &run_loop));
      run_loop.Run();
      EXPECT_TRUE(connection->GetRemoteApplicationID(&remote_id));
      EXPECT_NE(mojom::Connector::kInvalidApplicationID, remote_id);
    }

    {
      base::RunLoop run_loop;
      set_ping_callback(base::Bind(&QuitLoop, &run_loop));
      run_loop.Run();
      CompareConnectionState(
          kTestAppName, test_name(), test_userid(), test_instance_id(),
          kTestAppName, remote_id, connection->GetRemoteUserID());
    }
  }

  // Connect to an application provided by a package.
  {
    test::mojom::ConnectTestServicePtr service_a;
    scoped_ptr<Connection> connection = connector()->Connect(kTestAppAName);
    connection->GetInterface(&service_a);
    connection->AddInterface<test::mojom::ExposedInterface>(this);

    uint32_t remote_id = mojom::Connector::kInvalidApplicationID;
    {
      base::RunLoop run_loop;
      EXPECT_FALSE(connection->GetRemoteApplicationID(&remote_id));
      EXPECT_EQ(mojom::Connector::kInvalidApplicationID, remote_id);
      connection->AddRemoteIDCallback(base::Bind(&QuitLoop, &run_loop));
      run_loop.Run();
      EXPECT_TRUE(connection->GetRemoteApplicationID(&remote_id));
      EXPECT_NE(mojom::Connector::kInvalidApplicationID, remote_id);
    }

    {
      base::RunLoop run_loop;
      set_ping_callback(base::Bind(&QuitLoop, &run_loop));
      run_loop.Run();
      CompareConnectionState(
          kTestAppAName, test_name(), test_userid(), test_instance_id(),
          kTestAppAName, remote_id, connection->GetRemoteUserID());
    }

  }
}

}  // namespace shell
}  // namespace mojo
