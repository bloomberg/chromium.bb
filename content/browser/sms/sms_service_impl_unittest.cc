// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_service_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "content/browser/sms/sms_provider.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_service_manager_context.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/cpp/connector.h"
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

  MOCK_METHOD0(Retrieve, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSmsProvider);
};

class SmsServiceImplTest : public RenderViewHostTestHarness {
 protected:
  SmsServiceImplTest() {}

  ~SmsServiceImplTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SmsServiceImplTest);
};

}  // namespace

TEST_F(SmsServiceImplTest, Basic) {
  auto impl = std::make_unique<SmsServiceImpl>();
  auto* mock = new NiceMock<MockSmsProvider>();

  impl->SetSmsProviderForTest(base::WrapUnique(mock));

  blink::mojom::SmsManagerPtr service_ptr;
  GURL url("http://google.com");
  impl->Bind(mojo::MakeRequest(&service_ptr), url::Origin::Create(url));
  base::RunLoop loop;

  EXPECT_CALL(*mock, Retrieve()).WillOnce(Invoke([&mock, &url]() {
    mock->NotifyReceive(url::Origin::Create(url), "hi");
  }));

  service_ptr->GetNextMessage(
      base::TimeDelta::FromSeconds(10),
      base::BindLambdaForTesting([&](blink::mojom::SmsMessagePtr sms) {
        EXPECT_EQ("hi", sms->content);
        EXPECT_EQ(blink::mojom::SmsStatus::kSuccess, sms->status);
        loop.Quit();
      }));

  loop.Run();

  ASSERT_FALSE(mock->HasObservers());
}

TEST_F(SmsServiceImplTest, ExpectTwoReceiveTwoSerially) {
  auto impl = std::make_unique<SmsServiceImpl>();
  auto* mock = new NiceMock<MockSmsProvider>();

  impl->SetSmsProviderForTest(base::WrapUnique(mock));

  GURL url("http://google.com");

  EXPECT_CALL(*mock, Retrieve())
      .WillOnce(Invoke([&mock, &url]() {
        mock->NotifyReceive(url::Origin::Create(url), "first");
      }))
      .WillOnce(Invoke([&mock, &url]() {
        mock->NotifyReceive(url::Origin::Create(url), "second");
      }));

  blink::mojom::SmsManagerPtr service_ptr;
  impl->Bind(mojo::MakeRequest(&service_ptr), url::Origin::Create(url));

  {
    base::RunLoop loop;

    service_ptr->GetNextMessage(
        base::TimeDelta::FromSeconds(10),
        base::BindLambdaForTesting([&](blink::mojom::SmsMessagePtr sms) {
          EXPECT_EQ("first", sms->content);
          EXPECT_EQ(blink::mojom::SmsStatus::kSuccess, sms->status);
          loop.Quit();
        }));

    loop.Run();
  }

  {
    base::RunLoop loop;

    service_ptr->GetNextMessage(
        base::TimeDelta::FromSeconds(10),
        base::BindLambdaForTesting([&](blink::mojom::SmsMessagePtr sms) {
          EXPECT_EQ("second", sms->content);
          EXPECT_EQ(blink::mojom::SmsStatus::kSuccess, sms->status);
          loop.Quit();
        }));

    loop.Run();
  }
}

TEST_F(SmsServiceImplTest, IgnoreFromOtherOrigins) {
  auto impl = std::make_unique<SmsServiceImpl>();
  auto* mock = new NiceMock<MockSmsProvider>();

  impl->SetSmsProviderForTest(base::WrapUnique(mock));

  blink::mojom::SmsManagerPtr service_ptr;
  GURL url("http://a.com");
  url::Origin origin = url::Origin::Create(url);
  impl->Bind(mojo::MakeRequest(&service_ptr), origin);

  blink::mojom::SmsMessagePtr response;

  base::RunLoop listen_loop, sms_loop;

  EXPECT_CALL(*mock, Retrieve()).WillOnce(Invoke([&listen_loop]() {
    listen_loop.Quit();
  }));

  service_ptr->GetNextMessage(
      base::TimeDelta::FromSeconds(10),
      base::BindLambdaForTesting([&](blink::mojom::SmsMessagePtr sms) {
        response = std::move(sms);
        sms_loop.Quit();
      }));

  listen_loop.Run();

  // Delivers an SMS from an unrelated origin first and expect the manager to
  // ignore it.
  GURL another_url("http://b.com");
  mock->NotifyReceive(url::Origin::Create(another_url), "wrong");

  mock->NotifyReceive(origin, "right");

  sms_loop.Run();

  EXPECT_EQ("right", response->content);
  EXPECT_EQ(blink::mojom::SmsStatus::kSuccess, response->status);
}

TEST_F(SmsServiceImplTest, ExpectOneReceiveTwo) {
  auto impl = std::make_unique<SmsServiceImpl>();
  auto* mock = new NiceMock<MockSmsProvider>();

  impl->SetSmsProviderForTest(base::WrapUnique(mock));

  blink::mojom::SmsManagerPtr service_ptr;
  GURL url("http://a.com");
  url::Origin origin = url::Origin::Create(url);
  impl->Bind(mojo::MakeRequest(&service_ptr), origin);

  blink::mojom::SmsMessagePtr response;

  base::RunLoop listen_loop, sms_loop;

  EXPECT_CALL(*mock, Retrieve()).WillOnce(Invoke([&listen_loop]() {
    listen_loop.Quit();
  }));

  service_ptr->GetNextMessage(
      base::TimeDelta::FromSeconds(10),
      base::BindLambdaForTesting([&](blink::mojom::SmsMessagePtr sms) {
        response = std::move(sms);
        sms_loop.Quit();
      }));

  listen_loop.Run();

  // Delivers two SMSes for the same origin, even if only one was being
  // expected.
  mock->NotifyReceive(origin, "first");
  mock->NotifyReceive(origin, "second");

  sms_loop.Run();

  EXPECT_EQ("first", response->content);
  EXPECT_EQ(blink::mojom::SmsStatus::kSuccess, response->status);
}

TEST_F(SmsServiceImplTest, ExpectTwoReceiveTwoConcurrently) {
  auto impl = std::make_unique<SmsServiceImpl>();
  auto* mock = new NiceMock<MockSmsProvider>();

  impl->SetSmsProviderForTest(base::WrapUnique(mock));

  blink::mojom::SmsManagerPtr service_ptr;
  GURL url("http://a.com");
  url::Origin origin = url::Origin::Create(url);
  impl->Bind(mojo::MakeRequest(&service_ptr), origin);

  blink::mojom::SmsMessagePtr response1;
  blink::mojom::SmsMessagePtr response2;

  base::RunLoop listen_loop, sms1_loop, sms2_loop;

  // Expects two GetNextMessage() calls to be made before any of them gets
  // an SMS to resolve them.
  EXPECT_CALL(*mock, Retrieve())
      .WillOnce(testing::Return())
      .WillOnce(Invoke([&listen_loop]() { listen_loop.Quit(); }));

  service_ptr->GetNextMessage(
      base::TimeDelta::FromSeconds(10),
      base::BindLambdaForTesting([&](blink::mojom::SmsMessagePtr sms) {
        response1 = std::move(sms);
        sms1_loop.Quit();
      }));

  service_ptr->GetNextMessage(
      base::TimeDelta::FromSeconds(10),
      base::BindLambdaForTesting([&](blink::mojom::SmsMessagePtr sms) {
        response2 = std::move(sms);
        sms2_loop.Quit();
      }));

  listen_loop.Run();

  // Delivers the first SMS.

  mock->NotifyReceive(origin, "first");

  sms1_loop.Run();

  EXPECT_EQ("first", response1->content);
  EXPECT_EQ(blink::mojom::SmsStatus::kSuccess, response1->status);

  // Delivers the second SMS.

  mock->NotifyReceive(origin, "second");

  sms2_loop.Run();

  EXPECT_EQ("second", response2->content);
  EXPECT_EQ(blink::mojom::SmsStatus::kSuccess, response2->status);
}

TEST_F(SmsServiceImplTest, Timeout) {
  auto impl = std::make_unique<SmsServiceImpl>();
  auto* mock = new NiceMock<MockSmsProvider>();

  impl->SetSmsProviderForTest(base::WrapUnique(mock));

  blink::mojom::SmsManagerPtr service_ptr;
  GURL url("http://a.com");
  impl->Bind(mojo::MakeRequest(&service_ptr), url::Origin::Create(url));

  base::RunLoop loop;

  EXPECT_CALL(*mock, Retrieve()).WillOnce(Invoke([&mock]() {
    mock->NotifyTimeout();
  }));

  service_ptr->GetNextMessage(
      base::TimeDelta::FromSeconds(10),
      base::BindLambdaForTesting([&](blink::mojom::SmsMessagePtr sms) {
        EXPECT_EQ(blink::mojom::SmsStatus::kTimeout, sms->status);
        loop.Quit();
      }));

  loop.Run();
}

TEST_F(SmsServiceImplTest, TimeoutTwoOrigins) {
  auto impl = std::make_unique<SmsServiceImpl>();
  auto* mock = new NiceMock<MockSmsProvider>();

  impl->SetSmsProviderForTest(base::WrapUnique(mock));

  blink::mojom::SmsManagerPtr service_ptr1;
  GURL url1("http://a.com");
  impl->Bind(mojo::MakeRequest(&service_ptr1), url::Origin::Create(url1));

  blink::mojom::SmsManagerPtr service_ptr2;
  GURL url2("http://b.com");
  impl->Bind(mojo::MakeRequest(&service_ptr2), url::Origin::Create(url2));

  blink::mojom::SmsMessagePtr response1;
  blink::mojom::SmsMessagePtr response2;

  base::RunLoop listen, sms_loop1, sms_loop2;

  EXPECT_CALL(*mock, Retrieve())
      .WillOnce(testing::Return())
      .WillOnce(Invoke([&listen]() { listen.Quit(); }));

  service_ptr1->GetNextMessage(
      base::TimeDelta::FromSeconds(10),
      base::BindLambdaForTesting([&](blink::mojom::SmsMessagePtr sms) {
        response1 = std::move(sms);
        sms_loop1.Quit();
      }));

  service_ptr2->GetNextMessage(
      base::TimeDelta::FromSeconds(10),
      base::BindLambdaForTesting([&](blink::mojom::SmsMessagePtr sms) {
        response2 = std::move(sms);
        sms_loop2.Quit();
      }));

  listen.Run();

  // Timesout the first request.

  mock->NotifyTimeout();

  sms_loop1.Run();

  EXPECT_EQ(blink::mojom::SmsStatus::kTimeout, response1->status);

  // Delivers the second SMS.

  mock->NotifyReceive(url::Origin::Create(url2), "second");

  sms_loop2.Run();

  EXPECT_EQ("second", response2->content);
  EXPECT_EQ(blink::mojom::SmsStatus::kSuccess, response2->status);
}

}  // namespace content
