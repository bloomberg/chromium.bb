// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/idle/idle_manager.h"

#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/test/mock_permission_manager.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_service_manager_context.h"
#include "content/test/test_render_frame_host.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/idle/idle_manager.mojom.h"

using blink::mojom::IdleManagerPtr;
using blink::mojom::IdleMonitorPtr;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::StrictMock;

namespace content {

namespace {

const uint32_t kTresholdInSecs = 10;

class MockIdleMonitor : public blink::mojom::IdleMonitor {
 public:
  MOCK_METHOD1(Update, void(blink::mojom::IdleState));
};

class MockIdleTimeProvider : public IdleManager::IdleTimeProvider {
 public:
  MockIdleTimeProvider() = default;
  ~MockIdleTimeProvider() override = default;

  MOCK_METHOD1(CalculateIdleState, ui::IdleState(int));
  MOCK_METHOD0(CalculateIdleTime, int());
  MOCK_METHOD0(CheckIdleStateIsLocked, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockIdleTimeProvider);
};

class IdleManagerTest : public RenderViewHostImplTestHarness {
 protected:
  IdleManagerTest() {}

  ~IdleManagerTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(IdleManagerTest);
};

}  // namespace

TEST_F(IdleManagerTest, AddMonitor) {
  InSequence sequence;

  auto impl = std::make_unique<IdleManager>();
  blink::mojom::IdleManagerPtr service_ptr;
  GURL url("http://google.com");
  impl->CreateService(mojo::MakeRequest(&service_ptr),
                      url::Origin::Create(url));

  blink::mojom::IdleMonitorPtr monitor_ptr;
  blink::mojom::IdleMonitorRequest monitor_request =
      mojo::MakeRequest(&monitor_ptr);

  base::RunLoop loop;

  service_ptr.set_connection_error_handler(base::BindLambdaForTesting([&]() {
    ADD_FAILURE() << "Unexpected connection error";

    loop.Quit();
  }));

  service_ptr->AddMonitor(
      kTresholdInSecs, std::move(monitor_ptr),
      base::BindOnce(
          [](base::OnceClosure callback, blink::mojom::IdleState state) {
            // The initial state of the status of the user is to be active.
            EXPECT_EQ(blink::mojom::IdleState::ACTIVE, state);
            std::move(callback).Run();
          },
          loop.QuitClosure()));

  loop.Run();
}

TEST_F(IdleManagerTest, Update) {
  InSequence sequence;

  blink::mojom::IdleManagerPtr service_ptr;

  auto impl = std::make_unique<IdleManager>();
  auto* mock = new StrictMock<MockIdleTimeProvider>();
  impl->SetIdleTimeProviderForTest(base::WrapUnique(mock));

  GURL url("http://google.com");
  impl->CreateService(mojo::MakeRequest(&service_ptr),
                      url::Origin::Create(url));

  blink::mojom::IdleMonitorPtr monitor_ptr;
  auto monitor_request = mojo::MakeRequest(&monitor_ptr);
  MockIdleMonitor monitor;
  mojo::Binding<blink::mojom::IdleMonitor> monitor_binding(
      &monitor, std::move(monitor_request));

  base::RunLoop loop;

  service_ptr.set_connection_error_handler(base::BindLambdaForTesting([&]() {
    ADD_FAILURE() << "Unexpected connection error";

    loop.Quit();
  }));

  // Simulates a user going idle.
  EXPECT_CALL(*mock, CalculateIdleTime()).WillOnce(testing::Return(10));

  EXPECT_CALL(*mock, CheckIdleStateIsLocked()).WillOnce(testing::Return(false));

  // Expects Update to be notified about the change to idle.
  EXPECT_CALL(monitor, Update(_))
      .WillOnce(Invoke([](blink::mojom::IdleState state) {
        EXPECT_EQ(blink::mojom::IdleState::IDLE, state);
      }));

  // Simulates a user going active, calling a callback under the threshold.
  EXPECT_CALL(*mock, CalculateIdleTime()).WillOnce(testing::Return(1));

  EXPECT_CALL(*mock, CheckIdleStateIsLocked()).WillOnce(testing::Return(false));

  // Expects Update to be notified about the change to active.
  auto quit = loop.QuitClosure();
  EXPECT_CALL(monitor, Update(_))
      .WillOnce(Invoke([&quit](blink::mojom::IdleState state) {
        EXPECT_EQ(blink::mojom::IdleState::ACTIVE, state);
        // Ends the test.
        quit.Run();
      }));

  service_ptr->AddMonitor(kTresholdInSecs, std::move(monitor_ptr),
                          base::BindOnce([](blink::mojom::IdleState state) {
                            EXPECT_EQ(blink::mojom::IdleState::ACTIVE, state);
                          }));

  loop.Run();
}

}  // namespace content
