// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/tests/pickled_struct_blink.h"
#include "mojo/public/cpp/bindings/tests/pickled_struct_chromium.h"
#include "mojo/public/interfaces/bindings/tests/test_native_types.mojom-blink.h"
#include "mojo/public/interfaces/bindings/tests/test_native_types.mojom-chromium.h"
#include "mojo/public/interfaces/bindings/tests/test_native_types.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

template <typename T>
void DoExpectResult(int foo,
                    int bar,
                    const base::Closure& callback,
                    const T& actual) {
  EXPECT_EQ(foo, actual.foo());
  EXPECT_EQ(bar, actual.bar());
  callback.Run();
}

template <typename T>
base::Callback<void(const T&)> ExpectResult(const T& t,
                                            const base::Closure& callback) {
  return base::Bind(&DoExpectResult<T>, t.foo(), t.bar(), callback);
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
void ExpectError(InterfacePtr<T>* proxy, const base::Closure& callback) {
  proxy->set_connection_error_handler(callback);
}

// This implements the generated Chromium variant of PicklePasser.
class ChromiumPicklePasserImpl : public chromium::PicklePasser {
 public:
  ChromiumPicklePasserImpl() {}

  // mojo::test::chromium::PicklePasser:
  void PassPickle(const PickledStructChromium& pickle,
                  const PassPickleCallback& callback) override {
    callback.Run(pickle);
  }
};

// This implements the generated Blink variant of PicklePasser.
class BlinkPicklePasserImpl : public blink::PicklePasser {
 public:
  BlinkPicklePasserImpl() {}

  // mojo::test::blink::PicklePasser:
  void PassPickle(const PickledStructBlink& pickle,
                  const PassPickleCallback& callback) override {
    callback.Run(pickle);
  }
};

// A test which runs both Chromium and Blink implementations of the
// PicklePasser service.
class PickleTest : public testing::Test {
 public:
  PickleTest() {}

  void BindToChromiumService(mojo::InterfaceRequest<PicklePasser> request) {
    chromium_bindings_.AddBinding(&chromium_service_, std::move(request));
  }

  void BindToBlinkService(mojo::InterfaceRequest<PicklePasser> request) {
    blink_bindings_.AddBinding(&blink_service_, std::move(request));
  }

 private:
  base::MessageLoop loop_;
  ChromiumPicklePasserImpl chromium_service_;
  mojo::WeakBindingSet<chromium::PicklePasser> chromium_bindings_;
  BlinkPicklePasserImpl blink_service_;
  mojo::WeakBindingSet<blink::PicklePasser> blink_bindings_;
};

}  // namespace

TEST_F(PickleTest, ChromiumProxyToChromiumService) {
  chromium::PicklePasserPtr chromium_proxy;
  BindToChromiumService(GetProxy(&chromium_proxy));
  {
    base::RunLoop loop;
    chromium_proxy->PassPickle(
        PickledStructChromium(1, 2),
        ExpectResult(PickledStructChromium(1, 2), loop.QuitClosure()));
    loop.Run();
  }
  {
    base::RunLoop loop;
    chromium_proxy->PassPickle(
        PickledStructChromium(4, 5),
        ExpectResult(PickledStructChromium(4, 5), loop.QuitClosure()));
    loop.Run();
  }
}

TEST_F(PickleTest, ChromiumProxyToBlinkService) {
  chromium::PicklePasserPtr chromium_proxy;
  BindToBlinkService(GetProxy(&chromium_proxy));
  {
    base::RunLoop loop;
    chromium_proxy->PassPickle(
        PickledStructChromium(1, 2),
        ExpectResult(PickledStructChromium(1, 2), loop.QuitClosure()));
    loop.Run();
  }
  {
    base::RunLoop loop;
    chromium_proxy->PassPickle(
        PickledStructChromium(4, 5),
        ExpectResult(PickledStructChromium(4, 5), loop.QuitClosure()));
    loop.Run();
  }
  // The Blink service should drop our connection because the
  // PickledStructBlink ParamTraits deserializer rejects negative values.
  {
    base::RunLoop loop;
    chromium_proxy->PassPickle(
        PickledStructChromium(-1, -1),
        Fail<PickledStructChromium>("Blink service should reject this."));
    ExpectError(&chromium_proxy, loop.QuitClosure());
    loop.Run();
  }
}

TEST_F(PickleTest, BlinkProxyToBlinkService) {
  blink::PicklePasserPtr blink_proxy;
  BindToBlinkService(GetProxy(&blink_proxy));
  {
    base::RunLoop loop;
    blink_proxy->PassPickle(
        PickledStructBlink(1, 1),
        ExpectResult(PickledStructBlink(1, 1), loop.QuitClosure()));
    loop.Run();
  }
}

TEST_F(PickleTest, BlinkProxyToChromiumService) {
  blink::PicklePasserPtr blink_proxy;
  BindToChromiumService(GetProxy(&blink_proxy));
  {
    base::RunLoop loop;
    blink_proxy->PassPickle(
        PickledStructBlink(1, 1),
        ExpectResult(PickledStructBlink(1, 1), loop.QuitClosure()));
    loop.Run();
  }
}

}  // namespace test
}  // namespace mojo
