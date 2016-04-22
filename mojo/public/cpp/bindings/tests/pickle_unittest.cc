// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/tests/pickled_struct_blink.h"
#include "mojo/public/cpp/bindings/tests/pickled_struct_chromium.h"
#include "mojo/public/cpp/bindings/tests/variant_test_util.h"
#include "mojo/public/interfaces/bindings/tests/test_native_types.mojom-wtf.h"
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
class ChromiumPicklePasserImpl : public PicklePasser {
 public:
  ChromiumPicklePasserImpl() {}

  // mojo::test::PicklePasser:
  void PassPickle(const PickledStructChromium& pickle,
                  const PassPickleCallback& callback) override {
    callback.Run(pickle);
  }

  void PassPickleContainer(
      PickleContainerPtr container,
      const PassPickleContainerCallback& callback) override {
    callback.Run(std::move(container));
  }

  void PassPickles(Array<PickledStructChromium> pickles,
                   const PassPicklesCallback& callback) override {
    callback.Run(std::move(pickles));
  }

  void PassPickleArrays(Array<Array<PickledStructChromium>> pickle_arrays,
                        const PassPickleArraysCallback& callback) override {
    callback.Run(std::move(pickle_arrays));
  }
};

// This implements the generated Blink variant of PicklePasser.
class BlinkPicklePasserImpl : public wtf::PicklePasser {
 public:
  BlinkPicklePasserImpl() {}

  // mojo::test::wtf::PicklePasser:
  void PassPickle(const PickledStructBlink& pickle,
                  const PassPickleCallback& callback) override {
    callback.Run(pickle);
  }

  void PassPickleContainer(
      wtf::PickleContainerPtr container,
      const PassPickleContainerCallback& callback) override {
    callback.Run(std::move(container));
  }

  void PassPickles(WTFArray<PickledStructBlink> pickles,
                   const PassPicklesCallback& callback) override {
    callback.Run(std::move(pickles));
  }

  void PassPickleArrays(WTFArray<WTFArray<PickledStructBlink>> pickle_arrays,
                        const PassPickleArraysCallback& callback) override {
    callback.Run(std::move(pickle_arrays));
  }
};

// A test which runs both Chromium and Blink implementations of the
// PicklePasser service.
class PickleTest : public testing::Test {
 public:
  PickleTest() {}

  template <typename ProxyType = PicklePasser>
  InterfacePtr<ProxyType> ConnectToChromiumService() {
    InterfacePtr<ProxyType> proxy;
    InterfaceRequest<ProxyType> request = GetProxy(&proxy);
    chromium_bindings_.AddBinding(
        &chromium_service_,
        ConvertInterfaceRequest<PicklePasser>(std::move(request)));
    return proxy;
  }

  template <typename ProxyType = wtf::PicklePasser>
  InterfacePtr<ProxyType> ConnectToBlinkService() {
    InterfacePtr<ProxyType> proxy;
    InterfaceRequest<ProxyType> request = GetProxy(&proxy);
    blink_bindings_.AddBinding(
        &blink_service_,
        ConvertInterfaceRequest<wtf::PicklePasser>(std::move(request)));
    return proxy;
  }

 private:
  base::MessageLoop loop_;
  ChromiumPicklePasserImpl chromium_service_;
  BindingSet<PicklePasser> chromium_bindings_;
  BlinkPicklePasserImpl blink_service_;
  BindingSet<wtf::PicklePasser> blink_bindings_;
};

}  // namespace

TEST_F(PickleTest, ChromiumProxyToChromiumService) {
  auto chromium_proxy = ConnectToChromiumService();
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
  auto chromium_proxy = ConnectToBlinkService<PicklePasser>();
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
  auto blink_proxy = ConnectToBlinkService();
  {
    base::RunLoop loop;
    blink_proxy->PassPickle(
        PickledStructBlink(1, 1),
        ExpectResult(PickledStructBlink(1, 1), loop.QuitClosure()));
    loop.Run();
  }
}

TEST_F(PickleTest, BlinkProxyToChromiumService) {
  auto blink_proxy = ConnectToChromiumService<wtf::PicklePasser>();
  {
    base::RunLoop loop;
    blink_proxy->PassPickle(
        PickledStructBlink(1, 1),
        ExpectResult(PickledStructBlink(1, 1), loop.QuitClosure()));
    loop.Run();
  }
}

TEST_F(PickleTest, PickleArray) {
  auto proxy = ConnectToChromiumService();
  auto pickles = Array<PickledStructChromium>::New(2);
  pickles[0].set_foo(1);
  pickles[0].set_bar(2);
  pickles[0].set_baz(100);
  pickles[1].set_foo(3);
  pickles[1].set_bar(4);
  pickles[1].set_baz(100);
  {
    base::RunLoop run_loop;
    // Verify that the array of pickled structs can be serialized and
    // deserialized intact. This ensures that the ParamTraits are actually used
    // rather than doing a byte-for-byte copy of the element data, beacuse the
    // |baz| field should never be serialized.
    proxy->PassPickles(std::move(pickles),
                       [&](Array<PickledStructChromium> passed) {
                         ASSERT_FALSE(passed.is_null());
                         ASSERT_EQ(2u, passed.size());
                         EXPECT_EQ(1, passed[0].foo());
                         EXPECT_EQ(2, passed[0].bar());
                         EXPECT_EQ(0, passed[0].baz());
                         EXPECT_EQ(3, passed[1].foo());
                         EXPECT_EQ(4, passed[1].bar());
                         EXPECT_EQ(0, passed[1].baz());
                         run_loop.Quit();
                       });
    run_loop.Run();
  }
}

TEST_F(PickleTest, PickleArrayArray) {
  auto proxy = ConnectToChromiumService();
  auto pickle_arrays = Array<Array<PickledStructChromium>>::New(2);
  for (size_t i = 0; i < 2; ++i)
    pickle_arrays[i] = Array<PickledStructChromium>::New(2);

  pickle_arrays[0][0].set_foo(1);
  pickle_arrays[0][0].set_bar(2);
  pickle_arrays[0][0].set_baz(100);
  pickle_arrays[0][1].set_foo(3);
  pickle_arrays[0][1].set_bar(4);
  pickle_arrays[0][1].set_baz(100);
  pickle_arrays[1][0].set_foo(5);
  pickle_arrays[1][0].set_bar(6);
  pickle_arrays[1][0].set_baz(100);
  pickle_arrays[1][1].set_foo(7);
  pickle_arrays[1][1].set_bar(8);
  pickle_arrays[1][1].set_baz(100);
  {
    base::RunLoop run_loop;
    // Verify that the array-of-arrays serializes and deserializes properly.
    proxy->PassPickleArrays(std::move(pickle_arrays),
                            [&](Array<Array<PickledStructChromium>> passed) {
                              ASSERT_FALSE(passed.is_null());
                              ASSERT_EQ(2u, passed.size());
                              ASSERT_EQ(2u, passed[0].size());
                              ASSERT_EQ(2u, passed[1].size());
                              EXPECT_EQ(1, passed[0][0].foo());
                              EXPECT_EQ(2, passed[0][0].bar());
                              EXPECT_EQ(0, passed[0][0].baz());
                              EXPECT_EQ(3, passed[0][1].foo());
                              EXPECT_EQ(4, passed[0][1].bar());
                              EXPECT_EQ(0, passed[0][1].baz());
                              EXPECT_EQ(5, passed[1][0].foo());
                              EXPECT_EQ(6, passed[1][0].bar());
                              EXPECT_EQ(0, passed[1][0].baz());
                              EXPECT_EQ(7, passed[1][1].foo());
                              EXPECT_EQ(8, passed[1][1].bar());
                              EXPECT_EQ(0, passed[1][1].baz());
                              run_loop.Quit();
                            });
    run_loop.Run();
  }
}

TEST_F(PickleTest, PickleContainer) {
  auto proxy = ConnectToChromiumService();
  PickleContainerPtr pickle_container = PickleContainer::New();
  pickle_container->pickle.set_foo(42);
  pickle_container->pickle.set_bar(43);
  pickle_container->pickle.set_baz(44);
  EXPECT_TRUE(pickle_container.Equals(pickle_container));
  EXPECT_FALSE(pickle_container.Equals(PickleContainer::New()));
  {
    base::RunLoop run_loop;
    proxy->PassPickleContainer(std::move(pickle_container),
                               [&](PickleContainerPtr passed) {
                                 ASSERT_FALSE(passed.is_null());
                                 EXPECT_EQ(42, passed->pickle.foo());
                                 EXPECT_EQ(43, passed->pickle.bar());
                                 EXPECT_EQ(0, passed->pickle.baz());
                                 run_loop.Quit();
                               });
    run_loop.Run();
  }
}

}  // namespace test
}  // namespace mojo
