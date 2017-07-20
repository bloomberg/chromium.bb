// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "extensions/common/mojo/keep_alive.mojom.h"
#include "extensions/grit/extensions_renderer_resources.h"
#include "extensions/renderer/api_test_base.h"
#include "extensions/renderer/string_source_map.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

// A test launcher for tests for the stash client defined in
// extensions/test/data/keep_alive_client_unittest.js.

namespace extensions {
namespace {

// A KeepAlive implementation that calls provided callbacks on creation and
// destruction.
class TestKeepAlive : public KeepAlive {
 public:
  explicit TestKeepAlive(const base::Closure& on_destruction)
      : on_destruction_(on_destruction) {}

  ~TestKeepAlive() override { on_destruction_.Run(); }

  static void Create(const base::Closure& on_creation,
                     const base::Closure& on_destruction,
                     KeepAliveRequest keep_alive) {
    mojo::MakeStrongBinding(base::MakeUnique<TestKeepAlive>(on_destruction),
                            std::move(keep_alive));
    on_creation.Run();
  }

 private:
  const base::Closure on_destruction_;
};

const char kFakeSerialBindings[] =
    R"(
      var binding = apiBridge || require('binding').Binding.create('serial');
      var utils = require('utils');
      binding.registerCustomHook(function(bindingsAPI) {
        utils.handleRequestWithPromiseDoNotUse(bindingsAPI.apiFunctions,
                                               'serial', 'getDevices',
                                               function() {
          if (bindingsAPI.compiledApi.shouldSucceed) {
            return Promise.resolve([]);
          } else {
            return Promise.reject();
          }
        });
      });

      if (!apiBridge)
        exports.$set('binding', binding.generate());
    )";

}  // namespace

class KeepAliveClientTest : public ApiTestBase {
 public:
  KeepAliveClientTest() {}

  void SetUp() override {
    ApiTestBase::SetUp();
    interface_provider()->AddInterface(
        base::Bind(&TestKeepAlive::Create,
                   base::Bind(&KeepAliveClientTest::KeepAliveCreated,
                              base::Unretained(this)),
                   base::Bind(&KeepAliveClientTest::KeepAliveDestroyed,
                              base::Unretained(this))));

    // We register fake custom bindings for the serial API to use
    // handleRequestWithPromiseDoNotUse().
    env()->source_map()->RegisterModule("serial", kFakeSerialBindings);
  }

  void WaitForKeepAlive() {
    // Wait for a keep-alive to be created and destroyed.
    while (!created_keep_alive_ || !destroyed_keep_alive_) {
      base::RunLoop run_loop;
      stop_run_loop_ = run_loop.QuitClosure();
      run_loop.Run();
    }
    EXPECT_TRUE(created_keep_alive_);
    EXPECT_TRUE(destroyed_keep_alive_);
  }

 private:
  void KeepAliveCreated() {
    created_keep_alive_ = true;
    if (!stop_run_loop_.is_null())
      stop_run_loop_.Run();
  }
  void KeepAliveDestroyed() {
    destroyed_keep_alive_ = true;
    if (!stop_run_loop_.is_null())
      stop_run_loop_.Run();
  }

  bool created_keep_alive_ = false;
  bool destroyed_keep_alive_ = false;
  base::Closure stop_run_loop_;

  DISALLOW_COPY_AND_ASSIGN(KeepAliveClientTest);
};

TEST_F(KeepAliveClientTest, KeepAliveWithSuccessfulCall) {
  RunTest("keep_alive_client_unittest.js", "testKeepAliveWithSuccessfulCall");
  WaitForKeepAlive();
}

TEST_F(KeepAliveClientTest, KeepAliveWithError) {
  RunTest("keep_alive_client_unittest.js", "testKeepAliveWithError");
  WaitForKeepAlive();
}

}  // namespace extensions
