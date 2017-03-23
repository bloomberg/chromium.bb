// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mojo/blink_connector_impl.h"

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {
class MockConnector : public service_manager::Connector {
 public:
  MockConnector(int* foo_baz_impl_requests, int* bar_baz_impl_requests)
      : foo_baz_impl_requests_(foo_baz_impl_requests),
        bar_baz_impl_requests_(bar_baz_impl_requests),
        weak_factory_(this) {}

  // Connector:
  void StartService(const service_manager::Identity& identity,
                    service_manager::mojom::ServicePtr service,
                    service_manager::mojom::PIDReceiverRequest
                        pid_receiver_request) override {}
  std::unique_ptr<service_manager::Connection> Connect(
      const std::string& name) override {
    return nullptr;
  }
  std::unique_ptr<service_manager::Connection> Connect(
      const service_manager::Identity& target) override {
    return nullptr;
  }
  void BindInterface(const service_manager::Identity& target,
                     const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe) override;
  std::unique_ptr<Connector> Clone() override { return nullptr; }
  void BindConnectorRequest(
      service_manager::mojom::ConnectorRequest request) override {}
  base::WeakPtr<Connector> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

 protected:
  void OverrideBinderForTesting(const std::string& interface_name,
                                const TestApi::Binder& binder) override {}
  void ClearBinderOverrides() override {}

 private:
  int* foo_baz_impl_requests_;
  int* bar_baz_impl_requests_;
  base::WeakPtrFactory<MockConnector> weak_factory_;
};

void MockConnector::BindInterface(
    const service_manager::Identity& target,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  if (target.name() == "foo" && interface_name == "baz") {
    (*foo_baz_impl_requests_)++;
    return;
  }

  if (target.name() == "bar" && interface_name == "baz") {
    (*bar_baz_impl_requests_)++;
    return;
  }

  NOTREACHED();
}

}  // namespace

class BlinkConnectorImplTest : public testing::Test {
 public:
  BlinkConnectorImplTest()
      : foo_baz_impl_requests_(0),
        bar_baz_impl_requests_(0),
        foo_baz_override_requests_(0),
        bar_baz_override_requests_(0),
        connector_(base::MakeUnique<MockConnector>(&foo_baz_impl_requests_,
                                                   &bar_baz_impl_requests_)) {}

  void OverrideFooBaz(mojo::ScopedMessagePipeHandle interface_pipe) {
    foo_baz_override_requests_++;
  }

  void OverrideBarBaz(mojo::ScopedMessagePipeHandle interface_pipe) {
    bar_baz_override_requests_++;
  }

 protected:
  BlinkConnectorImpl* connector() { return &connector_; }
  int foo_baz_impl_requests() { return foo_baz_impl_requests_; }
  int bar_baz_impl_requests() { return bar_baz_impl_requests_; }
  int foo_baz_override_requests() { return foo_baz_override_requests_; }
  int bar_baz_override_requests() { return bar_baz_override_requests_; }

 private:
  base::MessageLoop loop_;
  int foo_baz_impl_requests_;
  int bar_baz_impl_requests_;
  int foo_baz_override_requests_;
  int bar_baz_override_requests_;
  BlinkConnectorImpl connector_;

  DISALLOW_COPY_AND_ASSIGN(BlinkConnectorImplTest);
};

TEST_F(BlinkConnectorImplTest, Basic) {
  EXPECT_EQ(0, foo_baz_impl_requests());
  EXPECT_EQ(0, bar_baz_impl_requests());

  mojo::MessagePipe pipe1;
  connector()->bindInterface("foo", "baz", std::move(pipe1.handle0));
  EXPECT_EQ(1, foo_baz_impl_requests());
  EXPECT_EQ(0, bar_baz_impl_requests());

  mojo::MessagePipe pipe2;
  connector()->bindInterface("bar", "baz", std::move(pipe2.handle0));
  EXPECT_EQ(1, foo_baz_impl_requests());
  EXPECT_EQ(1, bar_baz_impl_requests());
}

TEST_F(BlinkConnectorImplTest, Override) {
  EXPECT_EQ(0, foo_baz_impl_requests());
  EXPECT_EQ(0, foo_baz_override_requests());
  EXPECT_EQ(0, bar_baz_impl_requests());

  connector()->AddOverrideForTesting(
      "foo", "baz",
      base::Bind(&BlinkConnectorImplTest::OverrideFooBaz,
                 base::Unretained(this)));

  mojo::MessagePipe pipe1;
  connector()->bindInterface("foo", "baz", std::move(pipe1.handle0));
  EXPECT_EQ(0, foo_baz_impl_requests());
  EXPECT_EQ(1, foo_baz_override_requests());
  EXPECT_EQ(0, bar_baz_impl_requests());

  mojo::MessagePipe pipe2;
  connector()->bindInterface("bar", "baz", std::move(pipe2.handle0));
  EXPECT_EQ(0, foo_baz_impl_requests());
  EXPECT_EQ(1, foo_baz_override_requests());
  EXPECT_EQ(1, bar_baz_impl_requests());

  connector()->ClearOverridesForTesting();

  mojo::MessagePipe pipe3;
  connector()->bindInterface("foo", "baz", std::move(pipe3.handle0));
  EXPECT_EQ(1, foo_baz_impl_requests());
  EXPECT_EQ(1, foo_baz_override_requests());
  EXPECT_EQ(1, bar_baz_impl_requests());
}

}  // namespace content
