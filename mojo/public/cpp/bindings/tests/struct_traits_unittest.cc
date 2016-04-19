// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/tests/rect_blink.h"
#include "mojo/public/cpp/bindings/tests/rect_chromium.h"
#include "mojo/public/cpp/bindings/tests/struct_with_traits_impl.h"
#include "mojo/public/cpp/bindings/tests/variant_test_util.h"
#include "mojo/public/interfaces/bindings/tests/struct_with_traits.mojom.h"
#include "mojo/public/interfaces/bindings/tests/test_native_types.mojom-wtf.h"
#include "mojo/public/interfaces/bindings/tests/test_native_types.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

template <typename T>
void DoExpectResult(const T& expected,
                    const base::Closure& callback,
                    const T& actual) {
  EXPECT_EQ(expected.x(), actual.x());
  EXPECT_EQ(expected.y(), actual.y());
  EXPECT_EQ(expected.width(), actual.width());
  EXPECT_EQ(expected.height(), actual.height());
  callback.Run();
}

template <typename T>
base::Callback<void(const T&)> ExpectResult(const T& r,
                                            const base::Closure& callback) {
  return base::Bind(&DoExpectResult<T>, r, callback);
}

template <typename T>
void DoFail(const std::string& reason, const T&) {
  EXPECT_TRUE(false) << reason;
}

template <typename T>
base::Callback<void(const T&)> Fail(const std::string& reason) {
  return base::Bind(&DoFail<T>, reason);
}

template <typename T>
void ExpectError(InterfacePtr<T> *proxy, const base::Closure& callback) {
  proxy->set_connection_error_handler(callback);
}

// This implements the generated Chromium variant of RectService.
class ChromiumRectServiceImpl : public RectService {
 public:
  ChromiumRectServiceImpl() {}

  // mojo::test::RectService:
  void AddRect(const RectChromium& r) override {
    if (r.GetArea() > largest_rect_.GetArea())
      largest_rect_ = r;
  }

  void GetLargestRect(const GetLargestRectCallback& callback) override {
    callback.Run(largest_rect_);
  }

 private:
  RectChromium largest_rect_;
};

// This implements the generated Blink variant of RectService.
class BlinkRectServiceImpl : public wtf::RectService {
 public:
  BlinkRectServiceImpl() {}

  // mojo::test::wtf::RectService:
  void AddRect(const RectBlink& r) override {
    if (r.computeArea() > largest_rect_.computeArea()) {
      largest_rect_.setX(r.x());
      largest_rect_.setY(r.y());
      largest_rect_.setWidth(r.width());
      largest_rect_.setHeight(r.height());
    }
  }

  void GetLargestRect(const GetLargestRectCallback& callback) override {
    callback.Run(largest_rect_);
  }

 private:
  RectBlink largest_rect_;
};

// A test which runs both Chromium and Blink implementations of a RectService.
class StructTraitsTest : public testing::Test,
                         public TraitsTestService {
 public:
  StructTraitsTest() {}

 protected:
  void BindToChromiumService(RectServiceRequest request) {
    chromium_bindings_.AddBinding(&chromium_service_, std::move(request));
  }
  void BindToChromiumService(wtf::RectServiceRequest request) {
    chromium_bindings_.AddBinding(
        &chromium_service_,
        ConvertInterfaceRequest<RectService>(std::move(request)));
  }

  void BindToBlinkService(wtf::RectServiceRequest request) {
    blink_bindings_.AddBinding(&blink_service_, std::move(request));
  }
  void BindToBlinkService(RectServiceRequest request) {
    blink_bindings_.AddBinding(
        &blink_service_,
        ConvertInterfaceRequest<wtf::RectService>(std::move(request)));
  }

  TraitsTestServicePtr GetTraitsTestProxy() {
    return traits_test_bindings_.CreateInterfacePtrAndBind(this);
  }

 private:
  // TraitsTestService:
  void PassStructWithTraits(
      const StructWithTraitsImpl& s,
      const PassStructWithTraitsCallback& callback) override {
    callback.Run(s);
  }

  base::MessageLoop loop_;

  ChromiumRectServiceImpl chromium_service_;
  BindingSet<RectService> chromium_bindings_;

  BlinkRectServiceImpl blink_service_;
  BindingSet<wtf::RectService> blink_bindings_;

  BindingSet<TraitsTestService> traits_test_bindings_;
};

}  // namespace

TEST_F(StructTraitsTest, ChromiumProxyToChromiumService) {
  RectServicePtr chromium_proxy;
  BindToChromiumService(GetProxy(&chromium_proxy));
  {
    base::RunLoop loop;
    chromium_proxy->AddRect(RectChromium(1, 1, 4, 5));
    chromium_proxy->AddRect(RectChromium(-1, -1, 2, 2));
    chromium_proxy->GetLargestRect(
        ExpectResult(RectChromium(1, 1, 4, 5), loop.QuitClosure()));
    loop.Run();
  }
}

TEST_F(StructTraitsTest, ChromiumToBlinkService) {
  RectServicePtr chromium_proxy;
  BindToBlinkService(GetProxy(&chromium_proxy));
  {
    base::RunLoop loop;
    chromium_proxy->AddRect(RectChromium(1, 1, 4, 5));
    chromium_proxy->AddRect(RectChromium(2, 2, 5, 5));
    chromium_proxy->GetLargestRect(
        ExpectResult(RectChromium(2, 2, 5, 5), loop.QuitClosure()));
    loop.Run();
  }
  // The Blink service should drop our connection because RectBlink's
  // deserializer rejects negative origins.
  {
    base::RunLoop loop;
    ExpectError(&chromium_proxy, loop.QuitClosure());
    chromium_proxy->AddRect(RectChromium(-1, -1, 2, 2));
    chromium_proxy->GetLargestRect(
        Fail<RectChromium>("The pipe should have been closed."));
    loop.Run();
  }
}

TEST_F(StructTraitsTest, BlinkProxyToBlinkService) {
  wtf::RectServicePtr blink_proxy;
  BindToBlinkService(GetProxy(&blink_proxy));
  {
    base::RunLoop loop;
    blink_proxy->AddRect(RectBlink(1, 1, 4, 5));
    blink_proxy->AddRect(RectBlink(10, 10, 20, 20));
    blink_proxy->GetLargestRect(
        ExpectResult(RectBlink(10, 10, 20, 20), loop.QuitClosure()));
    loop.Run();
  }
}

TEST_F(StructTraitsTest, BlinkProxyToChromiumService) {
  wtf::RectServicePtr blink_proxy;
  BindToChromiumService(GetProxy(&blink_proxy));
  {
    base::RunLoop loop;
    blink_proxy->AddRect(RectBlink(1, 1, 4, 5));
    blink_proxy->AddRect(RectBlink(10, 10, 2, 2));
    blink_proxy->GetLargestRect(
        ExpectResult(RectBlink(1, 1, 4, 5), loop.QuitClosure()));
    loop.Run();
  }
}

TEST_F(StructTraitsTest, FieldTypes) {
  StructWithTraitsImpl input;
  input.set_bool(true);
  input.set_uint32(7);
  input.set_uint64(42);
  input.set_string("hello world!");

  base::RunLoop loop;
  TraitsTestServicePtr proxy = GetTraitsTestProxy();
  proxy->PassStructWithTraits(
      input,
      [&] (const StructWithTraitsImpl& passed) {
        EXPECT_EQ(input.get_bool(), passed.get_bool());
        EXPECT_EQ(input.get_uint32(), passed.get_uint32());
        EXPECT_EQ(input.get_uint64(), passed.get_uint64());
        EXPECT_EQ(input.get_string(), passed.get_string());
        loop.Quit();
      });
  loop.Run();
}

}  // namespace test
}  // namespace mojo
