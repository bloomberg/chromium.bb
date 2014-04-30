// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "gin/public/isolate_holder.h"
#include "mojo/apps/js/mojo_runner_delegate.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/common/test/test_utils.h"
#include "mojo/public/cpp/bindings/remote_ptr.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/cpp/system/macros.h"
#include "mojo/public/interfaces/bindings/tests/sample_interfaces.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace js {
namespace {

// Base Provider implementation class. It's expected that tests subclass and
// override the appropriate Provider functions. When test is done quit the
// run_loop().
class ProviderConnection : public sample::Provider {
 public:
  ProviderConnection() : run_loop_(NULL), client_(NULL) {
  }
  virtual ~ProviderConnection() {}

  void set_run_loop(base::RunLoop* run_loop) { run_loop_ = run_loop; }
  base::RunLoop* run_loop() { return run_loop_; }

  void set_client(sample::ProviderClient* client) { client_ = client; }
  sample::ProviderClient* client() { return client_; }

  // sample::Provider:
  virtual void EchoString(const String& a,
                          const Callback<void(String)>& callback) OVERRIDE {
    NOTREACHED();
  }
  virtual void EchoStrings(
      const String& a,
      const String& b,
      const Callback<void(String, String)>& callback) OVERRIDE {
    NOTREACHED();
  }
  virtual void EchoMessagePipeHandle(
      ScopedMessagePipeHandle a,
      const Callback<void(ScopedMessagePipeHandle)>& callback) OVERRIDE {
    NOTREACHED();
  }
  virtual void EchoEnum(sample::Enum a,
                        const Callback<void(sample::Enum)>& callback)
      OVERRIDE {
    NOTREACHED();
  }

 private:
  base::RunLoop* run_loop_;
  sample::ProviderClient* client_;

  DISALLOW_COPY_AND_ASSIGN(ProviderConnection);
};

class JsToCppTest : public testing::Test {
 public:
  JsToCppTest() {}

  void RunTest(const std::string& test, ProviderConnection* provider) {
    provider->set_run_loop(&run_loop_);
    InterfacePipe<sample::Provider, sample::ProviderClient> pipe;
    RemotePtr<sample::ProviderClient> provider_client;
    provider_client.reset(pipe.handle_to_peer.Pass(), provider);

    provider->set_client(provider_client.get());

    gin::IsolateHolder instance(gin::IsolateHolder::kStrictMode);
    apps::MojoRunnerDelegate delegate;
    gin::ShellRunner runner(&delegate, instance.isolate());
    delegate.Start(&runner, pipe.handle_to_self.release().value(),
                   test);

    run_loop_.Run();
  }

 private:
  Environment environment;
  base::MessageLoop loop;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(JsToCppTest);
};

// Trivial test to verify a message sent from JS is received.
class FromJsProviderConnection : public ProviderConnection {
 public:
  explicit FromJsProviderConnection() {}
  virtual ~FromJsProviderConnection() {
  }

  const base::string16& echo_string() const { return echo_string_; }

  // Provider:
  virtual void EchoString(const String& a,
                          const Callback<void(String)>& callback) OVERRIDE {
    echo_string_ = a.To<base::string16>();
    run_loop()->Quit();
  }

 private:
  base::string16 echo_string_;

  DISALLOW_COPY_AND_ASSIGN(FromJsProviderConnection);
};

TEST_F(JsToCppTest, FromJS) {
  // TODO(yzshen): Remove this check once isolated tests are supported on the
  // Chromium waterfall. (http://crbug.com/351214)
  const base::FilePath test_file_path(
      test::GetFilePathForJSResource(
          "mojo/public/interfaces/bindings/tests/sample_interfaces.mojom"));
  if (!base::PathExists(test_file_path)) {
    LOG(WARNING) << "Mojom binding files don't exist. Skipping the test.";
    return;
  }

  FromJsProviderConnection provider;
  RunTest("mojo/apps/js/test/js_to_cpp_unittest", &provider);
  EXPECT_EQ("message", base::UTF16ToASCII(provider.echo_string()));
}

}  // namespace
}  // namespace js
}  // namespace mojo
