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
#include "mojo/apps/js/test/js_to_cpp.mojom.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/common/test/test_utils.h"
#include "mojo/public/cpp/bindings/allocation_scope.h"
#include "mojo/public/cpp/bindings/remote_ptr.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/cpp/system/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace js {
namespace {

// Negative numbers with different values in each byte, the last of
// which can survive promotion to double and back.
const int8  kExpectedInt8Value = -65;
const int16 kExpectedInt16Value = -16961;
const int32 kExpectedInt32Value = -1145258561;
const int64 kExpectedInt64Value = -77263311946305LL;

// Positive numbers with different values in each byte, the last of
// which can survive promotion to double and back.
const uint8  kExpectedUInt8Value = 65;
const uint16 kExpectedUInt16Value = 16961;
const uint32 kExpectedUInt32Value = 1145258561;
const uint64 kExpectedUInt64Value = 77263311946305LL;

// Double/float values, including special case constants.
const double kExpectedDoubleVal = 3.14159265358979323846;
const double kExpectedDoubleInf = std::numeric_limits<double>::infinity();
const double kExpectedDoubleNan = std::numeric_limits<double>::quiet_NaN();
const float kExpectedFloatVal = static_cast<float>(kExpectedDoubleVal);
const float kExpectedFloatInf = std::numeric_limits<float>::infinity();
const float kExpectedFloatNan = std::numeric_limits<float>::quiet_NaN();

// NaN has the property that it is not equal to itself.
#define EXPECT_NAN(x) EXPECT_NE(x, x)

bool IsRunningOnIsolatedBot() {
  // TODO(yzshen): Remove this check once isolated tests are supported on the
  // Chromium waterfall. (http://crbug.com/351214)
  const base::FilePath test_file_path(
      test::GetFilePathForJSResource(
          "mojo/public/interfaces/bindings/tests/sample_interfaces.mojom"));
  if (!base::PathExists(test_file_path)) {
    LOG(WARNING) << "Mojom binding files don't exist. Skipping the test.";
    return true;
  }
  return false;
}

// NOTE: Callers will need to have established an AllocationScope, or you're
// gonna have a bad time.
js_to_cpp::EchoArgs BuildSampleEchoArgs() {
    js_to_cpp::EchoArgs::Builder builder;
    builder.set_si64(kExpectedInt64Value);
    builder.set_si32(kExpectedInt32Value);
    builder.set_si16(kExpectedInt16Value);
    builder.set_si8(kExpectedInt8Value);
    builder.set_ui64(kExpectedUInt64Value);
    builder.set_ui32(kExpectedUInt32Value);
    builder.set_ui16(kExpectedUInt16Value);
    builder.set_ui8(kExpectedUInt8Value);
    builder.set_float_val(kExpectedFloatVal);
    builder.set_float_inf(kExpectedFloatInf);
    builder.set_float_nan(kExpectedFloatNan);
    builder.set_double_val(kExpectedDoubleVal);
    builder.set_double_inf(kExpectedDoubleInf);
    builder.set_double_nan(kExpectedDoubleNan);
    builder.set_name("coming");
    mojo::Array<mojo::String>::Builder string_array(3);
    string_array[0] = "one";
    string_array[1] = "two";
    string_array[2] = "three";
    builder.set_string_array(string_array.Finish());
    return builder.Finish();
}

void CheckSampleEchoArgs(const js_to_cpp::EchoArgs& arg) {
    EXPECT_EQ(kExpectedInt64Value, arg.si64());
    EXPECT_EQ(kExpectedInt32Value, arg.si32());
    EXPECT_EQ(kExpectedInt16Value, arg.si16());
    EXPECT_EQ(kExpectedInt8Value, arg.si8());
    EXPECT_EQ(kExpectedUInt64Value, arg.ui64());
    EXPECT_EQ(kExpectedUInt32Value, arg.ui32());
    EXPECT_EQ(kExpectedUInt16Value, arg.ui16());
    EXPECT_EQ(kExpectedUInt8Value, arg.ui8());
    EXPECT_EQ(kExpectedFloatVal, arg.float_val());
    EXPECT_EQ(kExpectedFloatInf, arg.float_inf());
    EXPECT_NAN(arg.float_nan());
    EXPECT_EQ(kExpectedDoubleVal, arg.double_val());
    EXPECT_EQ(kExpectedDoubleInf, arg.double_inf());
    EXPECT_NAN(arg.double_nan());
    EXPECT_EQ(std::string("coming"), arg.name().To<std::string>());
    EXPECT_EQ(std::string("one"), arg.string_array()[0].To<std::string>());
    EXPECT_EQ(std::string("two"), arg.string_array()[1].To<std::string>());
    EXPECT_EQ(std::string("three"), arg.string_array()[2].To<std::string>());
}

// Base Provider implementation class. It's expected that tests subclass and
// override the appropriate Provider functions. When test is done quit the
// run_loop().
class CppSideConnection : public js_to_cpp::CppSide {
 public:
  CppSideConnection() : run_loop_(NULL), client_(NULL) {
  }
  virtual ~CppSideConnection() {}

  void set_run_loop(base::RunLoop* run_loop) { run_loop_ = run_loop; }
  base::RunLoop* run_loop() { return run_loop_; }

  void set_client(js_to_cpp::JsSide* client) { client_ = client; }
  js_to_cpp::JsSide* client() { return client_; }

  // js_to_cpp::CppSide:
  virtual void StartTest() OVERRIDE {
    NOTREACHED();
  }

  virtual void TestFinished() OVERRIDE {
    NOTREACHED();
  }

  virtual void PingResponse() OVERRIDE {
    NOTREACHED();
  }

  virtual void EchoResponse(const js_to_cpp::EchoArgs& arg1,
                            const js_to_cpp::EchoArgs& arg2) OVERRIDE {
    NOTREACHED();
  }

  virtual void BitFlipResponse(const js_to_cpp::EchoArgs& arg1) OVERRIDE {
    NOTREACHED();
  }

 protected:
  base::RunLoop* run_loop_;
  js_to_cpp::JsSide* client_;

 private:
  Environment environment;
  DISALLOW_COPY_AND_ASSIGN(CppSideConnection);
};

// Trivial test to verify a message sent from JS is received.
class PingCppSideConnection : public CppSideConnection {
 public:
  explicit PingCppSideConnection() : got_message_(false) {}
  virtual ~PingCppSideConnection() {}

  // js_to_cpp::CppSide:
  virtual void StartTest() OVERRIDE {
    client_->Ping();
  }

  virtual void PingResponse() OVERRIDE {
    got_message_ = true;
    run_loop()->Quit();
  }

  bool DidSucceed() {
    return got_message_;
  }

 private:
  bool got_message_;
  DISALLOW_COPY_AND_ASSIGN(PingCppSideConnection);
};

// Test that parameters are passed with correct values.
class EchoCppSideConnection : public CppSideConnection {
 public:
  explicit EchoCppSideConnection() :
      message_count_(0),
      termination_seen_(false) {
  }
  virtual ~EchoCppSideConnection() {}

  // js_to_cpp::CppSide:
  virtual void StartTest() OVERRIDE {
    AllocationScope scope;
    client_->Echo(kExpectedMessageCount, BuildSampleEchoArgs());
  }

  virtual void EchoResponse(const js_to_cpp::EchoArgs& arg1,
                            const js_to_cpp::EchoArgs& arg2) OVERRIDE {
    message_count_ += 1;
    CheckSampleEchoArgs(arg1);
    EXPECT_EQ(-1, arg2.si64());
    EXPECT_EQ(-1, arg2.si32());
    EXPECT_EQ(-1, arg2.si16());
    EXPECT_EQ(-1, arg2.si8());
    EXPECT_EQ(std::string("going"), arg2.name().To<std::string>());
  }

  virtual void TestFinished() OVERRIDE {
    termination_seen_ = true;
    run_loop()->Quit();
  }

  bool DidSucceed() {
    return termination_seen_ && message_count_ == kExpectedMessageCount;
  }

 private:
  static const int kExpectedMessageCount = 100;
  int message_count_;
  bool termination_seen_;
  DISALLOW_COPY_AND_ASSIGN(EchoCppSideConnection);
};

// Test that corrupted messages don't wreak havoc.
class BitFlipCppSideConnection : public CppSideConnection {
 public:
  explicit BitFlipCppSideConnection() : termination_seen_(false) {}
  virtual ~BitFlipCppSideConnection() {}

  // js_to_cpp::CppSide:
  virtual void StartTest() OVERRIDE {
    AllocationScope scope;
    client_->BitFlip(BuildSampleEchoArgs());
  }

  virtual void BitFlipResponse(const js_to_cpp::EchoArgs& arg1) OVERRIDE {
    // TODO(tsepez): How to check, may be corrupt in various ways.
  }

  virtual void TestFinished() OVERRIDE {
    termination_seen_ = true;
    run_loop()->Quit();
  }

  bool DidSucceed() {
    return termination_seen_;
  }

 private:
  bool termination_seen_;
  DISALLOW_COPY_AND_ASSIGN(BitFlipCppSideConnection);
};

}  // namespace

class JsToCppTest : public testing::Test {
 public:
  JsToCppTest() {}

  void RunTest(const std::string& test, CppSideConnection* cpp_side) {
    cpp_side->set_run_loop(&run_loop_);
    InterfacePipe<js_to_cpp::CppSide, js_to_cpp::JsSide> pipe;
    RemotePtr<js_to_cpp::JsSide> js_side;
    js_side.reset(pipe.handle_to_peer.Pass(), cpp_side);
    cpp_side->set_client(js_side.get());

    gin::IsolateHolder instance(gin::IsolateHolder::kStrictMode);
    apps::MojoRunnerDelegate delegate;
    gin::ShellRunner runner(&delegate, instance.isolate());
    delegate.Start(&runner, pipe.handle_to_self.release().value(),
                   test);

    run_loop_.Run();
  }

 private:
  base::MessageLoop loop;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(JsToCppTest);
};

TEST_F(JsToCppTest, Ping) {
  if (IsRunningOnIsolatedBot())
    return;

  PingCppSideConnection cpp_side_connection;
  RunTest("mojo/apps/js/test/js_to_cpp_unittest", &cpp_side_connection);
  EXPECT_TRUE(cpp_side_connection.DidSucceed());
}

TEST_F(JsToCppTest, Echo) {
  if (IsRunningOnIsolatedBot())
    return;

  EchoCppSideConnection cpp_side_connection;
  RunTest("mojo/apps/js/test/js_to_cpp_unittest", &cpp_side_connection);
  EXPECT_TRUE(cpp_side_connection.DidSucceed());
}

// TODO(tsepez): Disabled due to http://crbug.com/366797.
TEST_F(JsToCppTest, DISABLED_BitFlip) {
  if (IsRunningOnIsolatedBot())
    return;

  BitFlipCppSideConnection cpp_side_connection;
  RunTest("mojo/apps/js/test/js_to_cpp_unittest", &cpp_side_connection);
  EXPECT_TRUE(cpp_side_connection.DidSucceed());
}

}  // namespace js
}  // namespace mojo
