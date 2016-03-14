// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/shell/public/cpp/application_test_base.h"
#include "mojo/shell/public/cpp/shell_connection.h"
#include "mojo/shell/public/interfaces/shell_client.mojom.h"

namespace mojo {
namespace test {

namespace {

// Share the application name with multiple application tests.
shell::mojom::IdentityPtr g_identity;
uint32_t g_id = shell::mojom::kInvalidInstanceID;

// ShellClient request handle passed from the shell in MojoMain, stored in
// between SetUp()/TearDown() so we can (re-)intialize new ShellConnections.
InterfaceRequest<shell::mojom::ShellClient> g_shell_client_request;

// Connector pointer passed in the initial mojo.ShellClient.Initialize() call,
// stored in between initial setup and the first test and between SetUp/TearDown
// calls so we can (re-)initialize new ShellConnections.
shell::mojom::ConnectorPtr g_connector;

class ShellGrabber : public shell::mojom::ShellClient {
 public:
  explicit ShellGrabber(InterfaceRequest<shell::mojom::ShellClient> request)
      : binding_(this, std::move(request)) {
    binding_.set_connection_error_handler([] { _exit(1); });
  }

  void WaitForInitialize() {
    // Initialize is always the first call made on ShellClient.
    CHECK(binding_.WaitForIncomingMethodCall());
  }

 private:
  // shell::mojom::ShellClient implementation.
  void Initialize(shell::mojom::IdentityPtr identity,
                  uint32_t id,
                  const InitializeCallback& callback) override {
    callback.Run(GetProxy(&g_connector));

    g_identity = std::move(identity);
    g_id = id;
    g_shell_client_request = binding_.Unbind();
  }

  void AcceptConnection(
      shell::mojom::IdentityPtr source,
      uint32_t source_id,
      shell::mojom::InterfaceProviderRequest local_interfaces,
      shell::mojom::InterfaceProviderPtr remote_interfaces,
      shell::mojom::CapabilityRequestPtr capability_spec,
      const String& name) override {
    CHECK(false);
  }

  Binding<ShellClient> binding_;
};

void IgnoreConnectorRequest(shell::mojom::ConnectorRequest) {}

}  // namespace

MojoResult RunAllTests(MojoHandle shell_client_request_handle) {
  {
    // This loop is used for init, and then destroyed before running tests.
    base::MessageLoop message_loop;

    // Grab the shell handle.
    ShellGrabber grabber(
        MakeRequest<shell::mojom::ShellClient>(MakeScopedHandle(
            MessagePipeHandle(shell_client_request_handle))));
    grabber.WaitForInitialize();
    CHECK(g_connector);
    CHECK(g_shell_client_request.is_pending());

    int argc = 0;
    base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
    const char** argv = new const char* [cmd_line->argv().size() + 1];
#if defined(OS_WIN)
    std::vector<std::string> local_strings;
#endif
    for (auto& arg : cmd_line->argv()) {
#if defined(OS_WIN)
      local_strings.push_back(base::WideToUTF8(arg));
      argv[argc++] = local_strings.back().c_str();
#else
      argv[argc++] = arg.c_str();
#endif
    }
    argv[argc] = nullptr;

    testing::InitGoogleTest(&argc, const_cast<char**>(&(argv[0])));
  }

  int result = RUN_ALL_TESTS();

  // Shut down our message pipes before exiting.
  (void)g_shell_client_request.PassMessagePipe();
  g_connector.reset();

  return (result == 0) ? MOJO_RESULT_OK : MOJO_RESULT_UNKNOWN;
}

TestHelper::TestHelper(ShellClient* client)
    : shell_connection_(new ShellConnection(
          client == nullptr ? &default_shell_client_ : client,
          std::move(g_shell_client_request))),
      name_(g_identity->name),
      userid_(g_identity->user_id),
      instance_id_(g_id) {
  shell_connection_->SetAppTestConnectorForTesting(std::move(g_connector));

  // Fake ShellClient initialization.
  shell::mojom::ShellClient* shell_client = shell_connection_.get();
  shell_client->Initialize(std::move(g_identity), g_id,
                           base::Bind(&IgnoreConnectorRequest));
}

TestHelper::~TestHelper() {
  // We may have supplied a member as the client. Delete |shell_connection_|
  // while still valid.
  shell_connection_.reset();
}

ApplicationTestBase::ApplicationTestBase() : test_helper_(nullptr) {}

ApplicationTestBase::~ApplicationTestBase() {
}

ShellClient* ApplicationTestBase::GetShellClient() {
  return nullptr;
}

void ApplicationTestBase::SetUp() {
  // A run loop is recommended for ShellConnection initialization and
  // communication.
  if (ShouldCreateDefaultRunLoop()) {
    CHECK(!base::MessageLoop::current());
    // Not leaked: accessible from |base::MessageLoop::current()|.
    base::MessageLoop* message_loop = new base::MessageLoop();
    CHECK_EQ(message_loop, base::MessageLoop::current());
  }

  CHECK(g_shell_client_request.is_pending());
  CHECK(g_connector);

  // New applications are constructed for each test to avoid persisting state.
  test_helper_.reset(new TestHelper(GetShellClient()));
}

void ApplicationTestBase::TearDown() {
  CHECK(!g_shell_client_request.is_pending());
  CHECK(!g_connector);

  test_helper_.reset();

  if (ShouldCreateDefaultRunLoop()) {
    CHECK(base::MessageLoop::current());
    delete base::MessageLoop::current();
    CHECK(!base::MessageLoop::current());
  }
}

bool ApplicationTestBase::ShouldCreateDefaultRunLoop() {
  return true;
}

}  // namespace test
}  // namespace mojo
