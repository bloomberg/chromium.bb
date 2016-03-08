// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/test_suite.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/shell/public/cpp/names.h"
#include "mojo/shell/public/cpp/shell_test.h"
#include "mojo/shell/public/interfaces/shell.mojom.h"
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

class ConnectTest : public mojo::test::ShellTest,
                    public InterfaceFactory<test::mojom::ExposedInterface>,
                    public test::mojom::ExposedInterface {
 public:
  ConnectTest() : ShellTest("mojo:connect_unittests") {}
  ~ConnectTest() override {}

 protected:
  scoped_ptr<Connection> ConnectTo(Connector::ConnectParams* params) {
    scoped_ptr<Connection> connection = connector()->Connect(params);
    base::RunLoop loop;
    connection->AddConnectionCompletedClosure(base::Bind(&QuitLoop, &loop));
    loop.Run();
    return connection;
  }

  void set_ping_callback(const base::Closure& callback) {
    ping_callback_ = callback;
  }

  void CompareConnectionState(
      const std::string& connection_local_name,
      const std::string& connection_remote_name,
      const std::string& connection_remote_userid,
      uint32_t connection_remote_id,
      const std::string& initialize_local_name,
      const std::string& initialize_userid,
      uint32_t initialize_id) {
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
  // mojo::test::ShellTest:
  void SetUp() override {
    mojo::test::ShellTest::SetUp();
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

  DISALLOW_COPY_AND_ASSIGN(ConnectTest);
};

// Ensure the connection was properly established and that a round trip
// method call/response is completed.
TEST_F(ConnectTest, Connect) {
  scoped_ptr<Connection> connection = connector()->Connect(kTestAppName);
  test::mojom::ConnectTestServicePtr service;
  connection->GetInterface(&service);
  base::RunLoop run_loop;
  std::string title;
  service->GetTitle(base::Bind(&ReceiveTitle, &title, &run_loop));
  run_loop.Run();
  EXPECT_EQ("APP", title);
  EXPECT_FALSE(connection->IsPending());
  EXPECT_NE(mojom::kInvalidInstanceID, connection->GetRemoteInstanceID());
  EXPECT_EQ(connection->GetRemoteIdentity().name(), kTestAppName);
}

TEST_F(ConnectTest, Instances) {
  Connector::ConnectParams params_a(
      Identity(kTestAppName, mojom::kInheritUserID, "A"));
  scoped_ptr<Connection> connection_a1 = ConnectTo(&params_a);
  scoped_ptr<Connection> connection_a2 = ConnectTo(&params_a);
  std::string instance_a1, instance_a2;
  test::mojom::ConnectTestServicePtr service_a1;
  {
    connection_a1->GetInterface(&service_a1);
    base::RunLoop loop;
    service_a1->GetInstance(base::Bind(&ReceiveTitle, &instance_a1, &loop));
    loop.Run();
  }
  test::mojom::ConnectTestServicePtr service_a2;
  {
    connection_a2->GetInterface(&service_a2);
    base::RunLoop loop;
    service_a2->GetInstance(base::Bind(&ReceiveTitle, &instance_a2, &loop));
    loop.Run();
  }
  EXPECT_EQ(instance_a1, instance_a2);

  Connector::ConnectParams params_b(
      Identity(kTestAppName, mojom::kInheritUserID, "B"));
  scoped_ptr<Connection> connection_b = ConnectTo(&params_b);
  std::string instance_b;
  test::mojom::ConnectTestServicePtr service_b;
  {
    connection_b->GetInterface(&service_b);
    base::RunLoop loop;
    service_b->GetInstance(base::Bind(&ReceiveTitle, &instance_b, &loop));
    loop.Run();
  }

  EXPECT_NE(instance_a1, instance_b);
}

// When both the unresolved and resolved instance names are their default
// values, the instance name from the unresolved name must be used.
// (The case where the instance names differ is covered by
// LifecycleTest.PackagedApp_CrashCrashesOtherProvidedApp).
TEST_F(ConnectTest, PreferUnresolvedDefaultInstanceName) {
  // Connect to an app with no manifest-supplied instance name provided by a
  // package, the instance name must be derived from the application instance
  // name, not the package.
  scoped_ptr<Connection> connection = connector()->Connect(kTestAppName);
  {
    base::RunLoop loop;
    connection->AddConnectionCompletedClosure(base::Bind(&QuitLoop, &loop));
    loop.Run();
  }

  std::string instance;
  {
    test::mojom::ConnectTestServicePtr service;
    connection->GetInterface(&service);
    base::RunLoop loop;
    service->GetInstance(base::Bind(&ReceiveTitle, &instance, &loop));
    loop.Run();
  }
  EXPECT_EQ(GetNamePath(kTestAppName), instance);
}

// BlockedInterface should not be exposed to this application because it is not
// in our CapabilityFilter whitelist.
TEST_F(ConnectTest, BlockedInterface) {
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
TEST_F(ConnectTest, PackagedApp) {
  scoped_ptr<Connection> connection = connector()->Connect(kTestAppAName);
  test::mojom::ConnectTestServicePtr service_a;
  connection->GetInterface(&service_a);
  base::RunLoop run_loop;
  std::string a_name;
  service_a->GetTitle(base::Bind(&ReceiveTitle, &a_name, &run_loop));
  run_loop.Run();
  EXPECT_EQ("A", a_name);
  EXPECT_FALSE(connection->IsPending());
  EXPECT_NE(mojom::kInvalidInstanceID, connection->GetRemoteInstanceID());
  EXPECT_EQ(connection->GetRemoteIdentity().name(), kTestAppAName);
}

// Ask the target application to attempt to connect to a third application
// provided by a package whose id is permitted by the primary target's
// CapabilityFilter but whose package is not. The connection should be
// blocked and the returned title should be "uninitialized".
TEST_F(ConnectTest, BlockedPackage) {
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
TEST_F(ConnectTest, PackagedApp_BlockedInterface) {
  scoped_ptr<Connection> connection = connector()->Connect(kTestAppAName);
  base::RunLoop run_loop;
  test::mojom::BlockedInterfacePtr blocked;
  connection->GetInterface(&blocked);
  blocked.set_connection_error_handler(base::Bind(&QuitLoop, &run_loop));
  run_loop.Run();
}

// Connection to another application provided by the same package, blocked
// because it's not in the capability filter whitelist.
TEST_F(ConnectTest, BlockedPackagedApplication) {
  scoped_ptr<Connection> connection = connector()->Connect(kTestAppBName);
  test::mojom::ConnectTestServicePtr service_b;
  connection->GetInterface(&service_b);
  base::RunLoop run_loop;
  connection->SetConnectionLostClosure(base::Bind(&QuitLoop, &run_loop));
  run_loop.Run();
  EXPECT_FALSE(connection->IsPending());
  EXPECT_EQ(mojom::ConnectResult::ACCESS_DENIED, connection->GetResult());
  EXPECT_EQ(mojom::kInvalidInstanceID, connection->GetRemoteInstanceID());
}

// Tests that we can expose an interface to targets on outbound connections.
// TODO(beng): Currently all interfaces on outbound connections are exposed.
//             See ConnectorImpl::Connect().
TEST_F(ConnectTest, LocalInterface) {
  // Connect to a standalone application.
  {
    test::mojom::ConnectTestServicePtr service;
    scoped_ptr<Connection> connection = connector()->Connect(kTestAppName);
    connection->GetInterface(&service);
    connection->AddInterface<test::mojom::ExposedInterface>(this);

    uint32_t remote_id = shell::mojom::kInvalidInstanceID;
    {
      base::RunLoop run_loop;
      EXPECT_TRUE(connection->IsPending());
      EXPECT_EQ(mojom::kInvalidInstanceID, connection->GetRemoteInstanceID());
      connection->AddConnectionCompletedClosure(
          base::Bind(&QuitLoop, &run_loop));
      run_loop.Run();
      EXPECT_FALSE(connection->IsPending());
      remote_id = connection->GetRemoteInstanceID();
      EXPECT_NE(mojom::kInvalidInstanceID, remote_id);
    }

    {
      base::RunLoop run_loop;
      set_ping_callback(base::Bind(&QuitLoop, &run_loop));
      run_loop.Run();
      CompareConnectionState(
          kTestAppName, test_name(), test_userid(), test_instance_id(),
          kTestAppName, connection->GetRemoteIdentity().user_id(), remote_id);
    }
  }

  // Connect to an application provided by a package.
  {
    test::mojom::ConnectTestServicePtr service_a;
    scoped_ptr<Connection> connection = connector()->Connect(kTestAppAName);
    connection->GetInterface(&service_a);
    connection->AddInterface<test::mojom::ExposedInterface>(this);

    uint32_t remote_id = shell::mojom::kInvalidInstanceID;
    {
      base::RunLoop run_loop;
      EXPECT_TRUE(connection->IsPending());
      EXPECT_EQ(mojom::kInvalidInstanceID, connection->GetRemoteInstanceID());
      connection->AddConnectionCompletedClosure(
          base::Bind(&QuitLoop, &run_loop));
      run_loop.Run();
      EXPECT_FALSE(connection->IsPending());
      remote_id = connection->GetRemoteInstanceID();
      EXPECT_NE(mojom::kInvalidInstanceID, remote_id);
    }

    {
      base::RunLoop run_loop;
      set_ping_callback(base::Bind(&QuitLoop, &run_loop));
      run_loop.Run();
      CompareConnectionState(
          kTestAppAName, test_name(), test_userid(), test_instance_id(),
          kTestAppAName, connection->GetRemoteIdentity().user_id(), remote_id);
    }

  }
}

}  // namespace shell
}  // namespace mojo
