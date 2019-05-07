// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_service_manager_context.h"
#include "content/test/test_render_frame_host.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/cpp/connector.h"
#include "sms_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/sms/sms_manager.mojom.h"

using blink::mojom::SmsManagerPtr;
using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;

namespace content {

namespace {

class MockSmsProvider : public SmsProvider {
 public:
  MockSmsProvider() = default;
  ~MockSmsProvider() override = default;

  void Retrieve(base::TimeDelta timeout, SmsCallback callback) {
    DoRetrieve(timeout, &callback);
  }

  MOCK_METHOD2(DoRetrieve, void(base::TimeDelta, SmsCallback*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSmsProvider);
};

class SmsManagerTest : public RenderViewHostImplTestHarness {
 protected:
  SmsManagerTest() {}

  ~SmsManagerTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SmsManagerTest);
};

}  // namespace

TEST_F(SmsManagerTest, AddMonitor) {
  auto impl = std::make_unique<SmsManager>();
  auto mock = std::make_unique<NiceMock<MockSmsProvider>>();
  blink::mojom::SmsManagerPtr service_ptr;
  GURL url("http://google.com");
  impl->CreateService(mojo::MakeRequest(&service_ptr),
                      url::Origin::Create(url));
  base::RunLoop loop;

  service_ptr.set_connection_error_handler(base::BindLambdaForTesting([&]() {
    ADD_FAILURE() << "Unexpected connection error";

    loop.Quit();
  }));

  EXPECT_CALL(*mock, DoRetrieve(base::TimeDelta::FromSeconds(10), _))
      .WillOnce(Invoke(
          [](base::TimeDelta timeout,
             base::OnceCallback<void(bool, base::Optional<std::string> sms)>*
                 callback) { std::move(*callback).Run(true, "hi"); }));

  impl->SetSmsProviderForTest(std::move(mock));

  service_ptr->GetNextMessage(
      base::TimeDelta::FromSeconds(10),
      base::BindLambdaForTesting([&](blink::mojom::SmsMessagePtr sms) {
        // The initial state of the status of the user is to be active.
        EXPECT_EQ("hi", sms->content);
        EXPECT_EQ(blink::mojom::SmsStatus::kSuccess, sms->status);
        loop.Quit();
      }));

  loop.Run();
}

TEST_F(SmsManagerTest, InvalidArguments) {
  auto impl = std::make_unique<SmsManager>();
  auto mock = std::make_unique<NiceMock<MockSmsProvider>>();
  impl->SetSmsProviderForTest(std::move(mock));
  blink::mojom::SmsManagerPtr service_ptr;
  GURL url("http://google.com");
  impl->CreateService(mojo::MakeRequest(&service_ptr),
                      url::Origin::Create(url));
  service_ptr->GetNextMessage(base::TimeDelta::FromSeconds(-1),
                              base::NullCallback());

  mojo::test::BadMessageObserver bad_message_observer;
  EXPECT_EQ("Invalid timeout.", bad_message_observer.WaitForBadMessage());
}

}  // namespace content
