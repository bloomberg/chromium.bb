// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/fidl/cpp/binding.h>
#include <lib/fidl/cpp/internal/pending_response.h>
#include <lib/fidl/cpp/internal/weak_stub_controller.h>
#include <lib/zx/log.h>
#include <zircon/syscalls/log.h>

#include "base/bind.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/test/test_timeouts.h"
#include "build/fuchsia/fidlgen_js/fidl/fidljstest/cpp/fidl.h"
#include "build/fuchsia/fidlgen_js/runtime/zircon.h"
#include "gin/converter.h"
#include "gin/modules/console.h"
#include "gin/object_template_builder.h"
#include "gin/public/isolate_holder.h"
#include "gin/shell_runner.h"
#include "gin/test/v8_test.h"
#include "gin/try_catch.h"
#include "v8/include/v8.h"

static const char kRuntimeFile[] =
    "/pkg/build/fuchsia/fidlgen_js/runtime/fidl.mjs";
static const char kTestBindingFile[] =
    "/pkg/build/fuchsia/fidlgen_js/fidl/fidljstest/js/fidl.js";

namespace {

zx_koid_t GetKoidForHandle(const zx::object_base& object) {
  zx_info_handle_basic_t info;
  zx_status_t status =
      zx_object_get_info(object.get(), ZX_INFO_HANDLE_BASIC, &info,
                         sizeof(info), nullptr, nullptr);
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "zx_object_get_info";
    return ZX_KOID_INVALID;
  }
  return info.koid;
}

}  // namespace

class FidlGenJsTestShellRunnerDelegate : public gin::ShellRunnerDelegate {
 public:
  FidlGenJsTestShellRunnerDelegate() {}

  v8::Local<v8::ObjectTemplate> GetGlobalTemplate(
      gin::ShellRunner* runner,
      v8::Isolate* isolate) override {
    v8::Local<v8::ObjectTemplate> templ =
        gin::ObjectTemplateBuilder(isolate).Build();
    gin::Console::Register(isolate, templ);
    return templ;
  }

  void UnhandledException(gin::ShellRunner* runner,
                          gin::TryCatch& try_catch) override {
    LOG(ERROR) << try_catch.GetStackTrace();
    ADD_FAILURE();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FidlGenJsTestShellRunnerDelegate);
};

using FidlGenJsTest = gin::V8Test;

TEST_F(FidlGenJsTest, BasicJSSetup) {
  v8::Isolate* isolate = instance_->isolate();

  std::string source = "log('this is a log'); this.stuff = 'HAI';";
  FidlGenJsTestShellRunnerDelegate delegate;
  gin::ShellRunner runner(&delegate, isolate);
  gin::Runner::Scope scope(&runner);
  runner.Run(source, "test.js");

  std::string result;
  EXPECT_TRUE(gin::Converter<std::string>::FromV8(
      isolate, runner.global()->Get(gin::StringToV8(isolate, "stuff")),
      &result));
  EXPECT_EQ("HAI", result);
}

void LoadAndSource(gin::ShellRunner* runner, const base::FilePath& filename) {
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(filename, &contents));

  runner->Run(contents, filename.MaybeAsASCII());
}

class BindingsSetupHelper {
 public:
  explicit BindingsSetupHelper(v8::Isolate* isolate)
      : isolate_(isolate),
        handle_scope_(isolate),
        delegate_(),
        runner_(&delegate_, isolate),
        scope_(&runner_),
        zx_bindings_(
            std::make_unique<fidljs::ZxBindings>(isolate, runner_.global())) {
    // TODO(scottmg): Figure out how to set up v8 import hooking and make
    // fidl_Xyz into $fidl.Xyz. Manually inject the runtime support js files
    // for now. https://crbug.com/883496.
    LoadAndSource(&runner_, base::FilePath(kRuntimeFile));
    LoadAndSource(&runner_, base::FilePath(kTestBindingFile));

    zx_status_t status = zx::channel::create(0, &server_, &client_);
    EXPECT_EQ(status, ZX_OK);

    runner_.global()->Set(gin::StringToSymbol(isolate, "testHandle"),
                          gin::ConvertToV8(isolate, client_.get()));
  }

  template <class T>
  T Get(const std::string& name) {
    T t;
    EXPECT_TRUE(gin::Converter<T>::FromV8(
        isolate_, runner_.global()->Get(gin::StringToV8(isolate_, name)), &t));
    return t;
  }

  void DestroyBindingsForTesting() { zx_bindings_.reset(); }

  zx::channel& server() { return server_; }
  zx::channel& client() { return client_; }
  gin::ShellRunner& runner() { return runner_; }

 private:
  v8::Isolate* isolate_;
  v8::HandleScope handle_scope_;
  FidlGenJsTestShellRunnerDelegate delegate_;
  gin::ShellRunner runner_;
  gin::Runner::Scope scope_;
  std::unique_ptr<fidljs::ZxBindings> zx_bindings_;
  zx::channel server_;
  zx::channel client_;

  DISALLOW_COPY_AND_ASSIGN(BindingsSetupHelper);
};

class TestolaImpl : public fidljstest::Testola {
 public:
  TestolaImpl() {
    // Don't want the default values from the C++ side.
    memset(&basic_struct_, -1, sizeof(basic_struct_));
  }
  ~TestolaImpl() override {}

  void DoSomething() override { was_do_something_called_ = true; }

  void PrintInt(int32_t number) override { received_int_ = number; }

  void PrintMsg(fidl::StringPtr message) override {
    std::string as_str = message.get();
    received_msg_ = as_str;
  }

  void VariousArgs(fidljstest::Blorp blorp,
                   fidl::StringPtr msg,
                   fidl::VectorPtr<uint32_t> stuff) override {
    std::string msg_as_str = msg.get();
    std::vector<uint32_t> stuff_as_vec = stuff.get();
    various_blorp_ = blorp;
    various_msg_ = msg_as_str;
    various_stuff_ = stuff_as_vec;
  }

  void WithResponse(int32_t a,
                    int32_t b,
                    WithResponseCallback callback) override {
    response_callbacks_.push_back(base::BindOnce(
        [](WithResponseCallback callback, int32_t result) { callback(result); },
        std::move(callback), a + b));
  }

  void SendAStruct(fidljstest::BasicStruct basic_struct) override {
    basic_struct_ = basic_struct;
  }

  void NestedStructsWithResponse(
      fidljstest::BasicStruct basic_struct,
      NestedStructsWithResponseCallback resp) override {
    // Construct a response, echoing the passed in structure with some
    // modifications, as well as additional data.
    fidljstest::StuffAndThings sat;
    sat.count = 123;
    sat.id = "here is my id";
    sat.a_vector.push_back(1);
    sat.a_vector.push_back(-2);
    sat.a_vector.push_back(4);
    sat.a_vector.push_back(-8);
    sat.basic.b = !basic_struct.b;
    sat.basic.i8 = basic_struct.i8 * 2;
    sat.basic.i16 = basic_struct.i16 * 2;
    sat.basic.i32 = basic_struct.i32 * 2;
    sat.basic.u8 = basic_struct.u8 * 2;
    sat.basic.u16 = basic_struct.u16 * 2;
    sat.basic.u32 = basic_struct.u32 * 2;
    sat.later_string = "ⓣⓔⓡⓜⓘⓝⓐⓣⓞⓡ";
    for (uint64_t i = 0; i < fidljstest::ARRRR_SIZE; ++i) {
      sat.arrrr[i] = static_cast<int32_t>(i * 5) - 10;
    }

    resp(std::move(sat));
  }

  void PassHandles(zx::job job, PassHandlesCallback callback) override {
    EXPECT_EQ(GetKoidForHandle(job), GetKoidForHandle(*zx::job::default_job()));
    zx::log log;
    EXPECT_EQ(zx::log::create(ZX_LOG_FLAG_READABLE, &log), ZX_OK);
    unowned_log_handle_ = log.get();
    callback(std::move(log));
  }

  void ReceiveUnions(fidljstest::StructOfMultipleUnions somu) override {
    EXPECT_TRUE(somu.initial.is_swb());
    EXPECT_TRUE(somu.initial.swb().some_bool);

    EXPECT_TRUE(somu.optional.get());
    EXPECT_TRUE(somu.optional->is_lswa());
    for (int i = 0; i < 32; ++i) {
      EXPECT_EQ(somu.optional->lswa().components[i], i * 99);
    }

    EXPECT_TRUE(somu.trailing.is_swu());
    EXPECT_EQ(somu.trailing.swu().num, 123456u);

    did_receive_union_ = true;
  }

  void SendUnions(SendUnionsCallback callback) override {
    fidljstest::StructOfMultipleUnions resp;

    resp.initial.set_swb(fidljstest::StructWithBool());
    resp.initial.swb().some_bool = true;

    resp.optional = std::make_unique<fidljstest::UnionOfStructs>();
    resp.optional->set_swu(fidljstest::StructWithUint());
    resp.optional->swu().num = 987654;

    resp.trailing.set_lswa(fidljstest::LargerStructWithArray());

    callback(std::move(resp));
  }

  void SendVectorsOfString(
      fidl::VectorPtr<fidl::StringPtr> unsized,
      fidl::VectorPtr<fidl::StringPtr> nullable,
      fidl::VectorPtr<fidl::StringPtr> max_strlen) override {
    ASSERT_EQ(unsized->size(), 3u);
    EXPECT_EQ((*unsized)[0], "str0");
    EXPECT_EQ((*unsized)[1], "str1");
    EXPECT_EQ((*unsized)[2], "str2");

    ASSERT_EQ(nullable->size(), 5u);
    EXPECT_EQ((*nullable)[0], "str3");
    EXPECT_TRUE((*nullable)[1].is_null());
    EXPECT_TRUE((*nullable)[2].is_null());
    EXPECT_TRUE((*nullable)[3].is_null());
    EXPECT_EQ((*nullable)[4], "str4");

    ASSERT_EQ(max_strlen->size(), 1u);
    EXPECT_EQ((*max_strlen)[0], "0123456789");

    did_get_vectors_of_string_ = true;
  }

  void VectorOfStruct(fidl::VectorPtr<fidljstest::StructWithUint> stuff,
                      VectorOfStructCallback callback) override {
    ASSERT_EQ(stuff->size(), 4u);
    EXPECT_EQ((*stuff)[0].num, 456u);
    EXPECT_EQ((*stuff)[1].num, 789u);
    EXPECT_EQ((*stuff)[2].num, 123u);
    EXPECT_EQ((*stuff)[3].num, 0xfffffu);

    fidl::VectorPtr<fidljstest::StructWithUint> response;
    fidljstest::StructWithUint a;
    a.num = 369;
    response.push_back(a);
    fidljstest::StructWithUint b;
    b.num = 258;
    response.push_back(b);
    callback(std::move(response));
  }

  bool was_do_something_called() const { return was_do_something_called_; }
  int32_t received_int() const { return received_int_; }
  const std::string& received_msg() const { return received_msg_; }

  fidljstest::Blorp various_blorp() const { return various_blorp_; }
  const std::string& various_msg() const { return various_msg_; }
  const std::vector<uint32_t>& various_stuff() const { return various_stuff_; }

  zx_handle_t unowned_log_handle() const { return unowned_log_handle_; }

  fidljstest::BasicStruct GetReceivedStruct() const { return basic_struct_; }

  bool did_receive_union() const { return did_receive_union_; }

  bool did_get_vectors_of_string() const { return did_get_vectors_of_string_; }

  void CallResponseCallbacks() {
    for (auto& cb : response_callbacks_) {
      std::move(cb).Run();
    }
    response_callbacks_.clear();
  }

 private:
  bool was_do_something_called_ = false;
  int32_t received_int_ = -1;
  std::string received_msg_;
  fidljstest::Blorp various_blorp_;
  std::string various_msg_;
  std::vector<uint32_t> various_stuff_;
  fidljstest::BasicStruct basic_struct_;
  std::vector<base::OnceClosure> response_callbacks_;
  zx_handle_t unowned_log_handle_;
  bool did_receive_union_ = false;
  bool did_get_vectors_of_string_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestolaImpl);
};

TEST_F(FidlGenJsTest, RawReceiveFidlMessage) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  // Send the data from the JS side into the channel.
  std::string source = R"(
    var proxy = new TestolaProxy();
    proxy.$bind(testHandle);
    proxy.DoSomething();
  )";
  helper.runner().Run(source, "test.js");

  // Read it out, decode, and confirm it was dispatched.
  TestolaImpl testola_impl;
  fidljstest::Testola_Stub stub(&testola_impl);
  uint8_t data[1024];
  zx_handle_t handles[1];
  uint32_t actual_bytes, actual_handles;
  ASSERT_EQ(helper.server().read(0, data, base::size(data), &actual_bytes,
                                 handles, base::size(handles), &actual_handles),
            ZX_OK);
  EXPECT_EQ(actual_bytes, 16u);
  EXPECT_EQ(actual_handles, 0u);

  fidl::Message message(
      fidl::BytePart(data, actual_bytes, actual_bytes),
      fidl::HandlePart(handles, actual_handles, actual_handles));
  stub.Dispatch_(std::move(message), fidl::internal::PendingResponse());

  EXPECT_TRUE(testola_impl.was_do_something_called());
}

TEST_F(FidlGenJsTest, RawReceiveFidlMessageWithSimpleArg) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  // Send the data from the JS side into the channel.
  std::string source = R"(
    var proxy = new TestolaProxy();
    proxy.$bind(testHandle);
    proxy.PrintInt(12345);
  )";
  helper.runner().Run(source, "test.js");

  // Read it out, decode, and confirm it was dispatched.
  TestolaImpl testola_impl;
  fidljstest::Testola_Stub stub(&testola_impl);
  uint8_t data[1024];
  zx_handle_t handles[1];
  uint32_t actual_bytes, actual_handles;
  ASSERT_EQ(helper.server().read(0, data, base::size(data), &actual_bytes,
                                 handles, base::size(handles), &actual_handles),
            ZX_OK);
  // 24 rather than 20 because everything's 8 aligned.
  EXPECT_EQ(actual_bytes, 24u);
  EXPECT_EQ(actual_handles, 0u);

  fidl::Message message(
      fidl::BytePart(data, actual_bytes, actual_bytes),
      fidl::HandlePart(handles, actual_handles, actual_handles));
  stub.Dispatch_(std::move(message), fidl::internal::PendingResponse());

  EXPECT_EQ(testola_impl.received_int(), 12345);
}

TEST_F(FidlGenJsTest, RawReceiveFidlMessageWithStringArg) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  // Send the data from the JS side into the channel.
  std::string source = R"(
    var proxy = new TestolaProxy();
    proxy.$bind(testHandle);
    proxy.PrintMsg('Ça c\'est a 你好 from deep in JS');
  )";
  helper.runner().Run(source, "test.js");

  // Read it out, decode, and confirm it was dispatched.
  TestolaImpl testola_impl;
  fidljstest::Testola_Stub stub(&testola_impl);
  uint8_t data[1024];
  zx_handle_t handles[1];
  uint32_t actual_bytes, actual_handles;
  ASSERT_EQ(helper.server().read(0, data, base::size(data), &actual_bytes,
                                 handles, base::size(handles), &actual_handles),
            ZX_OK);
  EXPECT_EQ(actual_handles, 0u);

  fidl::Message message(
      fidl::BytePart(data, actual_bytes, actual_bytes),
      fidl::HandlePart(handles, actual_handles, actual_handles));
  stub.Dispatch_(std::move(message), fidl::internal::PendingResponse());

  EXPECT_EQ(testola_impl.received_msg(), "Ça c'est a 你好 from deep in JS");
}

TEST_F(FidlGenJsTest, RawReceiveFidlMessageWithMultipleArgs) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  // Send the data from the JS side into the channel.
  std::string source = R"(
    var proxy = new TestolaProxy();
    proxy.$bind(testHandle);
    proxy.VariousArgs(Blorp.GAMMA, 'zippy zap', [ 999, 987, 123456 ]);
  )";
  helper.runner().Run(source, "test.js");

  // Read it out, decode, and confirm it was dispatched.
  TestolaImpl testola_impl;
  fidljstest::Testola_Stub stub(&testola_impl);
  uint8_t data[1024];
  zx_handle_t handles[1];
  uint32_t actual_bytes, actual_handles;
  ASSERT_EQ(helper.server().read(0, data, base::size(data), &actual_bytes,
                                 handles, base::size(handles), &actual_handles),
            ZX_OK);
  EXPECT_EQ(actual_handles, 0u);

  fidl::Message message(
      fidl::BytePart(data, actual_bytes, actual_bytes),
      fidl::HandlePart(handles, actual_handles, actual_handles));
  stub.Dispatch_(std::move(message), fidl::internal::PendingResponse());

  EXPECT_EQ(testola_impl.various_blorp(), fidljstest::Blorp::GAMMA);
  EXPECT_EQ(testola_impl.various_msg(), "zippy zap");
  ASSERT_EQ(testola_impl.various_stuff().size(), 3u);
  EXPECT_EQ(testola_impl.various_stuff()[0], 999u);
  EXPECT_EQ(testola_impl.various_stuff()[1], 987u);
  EXPECT_EQ(testola_impl.various_stuff()[2], 123456u);
}

TEST_F(FidlGenJsTest, RawWithResponse) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  TestolaImpl testola_impl;
  fidl::Binding<fidljstest::Testola> binding(&testola_impl);
  binding.Bind(std::move(helper.server()));

  // Send the data from the JS side into the channel.
  std::string source = R"(
      var proxy = new TestolaProxy();
      proxy.$bind(testHandle);
      this.sum_result = -1;
      proxy.WithResponse(72, 99)
           .then(sum => {
              this.sum_result = sum;
            })
           .catch((e) => log('FAILED: ' + e));
    )";
  helper.runner().Run(source, "test.js");

  base::RunLoop().RunUntilIdle();

  testola_impl.CallResponseCallbacks();

  base::RunLoop().RunUntilIdle();

  // Confirm that the response was received with the correct value.
  auto sum_result = helper.Get<int>("sum_result");
  EXPECT_EQ(sum_result, 72 + 99);
}

TEST_F(FidlGenJsTest, NoResponseBeforeTearDown) {
  v8::Isolate* isolate = instance_->isolate();

  BindingsSetupHelper helper(isolate);

  TestolaImpl testola_impl;
  fidl::Binding<fidljstest::Testola> binding(&testola_impl);
  binding.Bind(std::move(helper.server()));

  // Send the data from the JS side into the channel.
  std::string source = R"(
      var proxy = new TestolaProxy();
      proxy.$bind(testHandle);
      this.resolved = false;
      this.rejected = false;
      this.excepted = false;
      proxy.WithResponse(1, 2)
           .then(sum => {
              this.resolved = true;
            }, () => {
              this.rejected = true;
            })
           .catch((e) => {
             log('FAILED: ' + e);
             this.excepted = true;
           })
    )";
  helper.runner().Run(source, "test.js");

  // Run the message loop to read and queue the request, but don't send the
  // response.
  base::RunLoop().RunUntilIdle();

  // This causes outstanding waits to be canceled.
  helper.DestroyBindingsForTesting();

  EXPECT_FALSE(helper.Get<bool>("resolved"));
  EXPECT_TRUE(helper.Get<bool>("rejected"));
  EXPECT_FALSE(helper.Get<bool>("excepted"));
}

TEST_F(FidlGenJsTest, RawReceiveFidlStructMessage) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  TestolaImpl testola_impl;
  fidl::Binding<fidljstest::Testola> binding(&testola_impl);
  binding.Bind(std::move(helper.server()));

  // Send the data from the JS side into the channel.
  std::string source = R"(
    var proxy = new TestolaProxy();
    proxy.$bind(testHandle);
    var basicStruct = new BasicStruct(
        true, -30, undefined, -789, 200, 65000, 0);
    proxy.SendAStruct(basicStruct);
  )";
  helper.runner().Run(source, "test.js");

  // Run the dispatcher to read and dispatch the response.
  base::RunLoop().RunUntilIdle();

  fidljstest::BasicStruct received_struct = testola_impl.GetReceivedStruct();
  EXPECT_EQ(received_struct.b, true);
  EXPECT_EQ(received_struct.i8, -30);
  EXPECT_EQ(received_struct.i16, 18);  // From defaults.
  EXPECT_EQ(received_struct.i32, -789);
  EXPECT_EQ(received_struct.u8, 200);
  EXPECT_EQ(received_struct.u16, 65000);
  // Make sure this didn't get defaulted, even though it has a false-ish value.
  EXPECT_EQ(received_struct.u32, 0u);
}

TEST_F(FidlGenJsTest, RawReceiveFidlNestedStructsAndRespond) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  TestolaImpl testola_impl;
  fidl::Binding<fidljstest::Testola> binding(&testola_impl);
  binding.Bind(std::move(helper.server()));

  // Send the data from the JS side into the channel.
  std::string source = R"(
      var proxy = new TestolaProxy();
      proxy.$bind(testHandle);
      var toSend = new BasicStruct(false, -5, -6, -7, 8, 32000, 2000000000);
      proxy.NestedStructsWithResponse(toSend)
           .then(sat => {
             this.result_count = sat.count;
             this.result_id = sat.id;
             this.result_vector = sat.a_vector;
             this.result_basic_b = sat.basic.b;
             this.result_basic_i8 = sat.basic.i8;
             this.result_basic_i16 = sat.basic.i16;
             this.result_basic_i32 = sat.basic.i32;
             this.result_basic_u8 = sat.basic.u8;
             this.result_basic_u16 = sat.basic.u16;
             this.result_basic_u32 = sat.basic.u32;
             this.result_later_string = sat.later_string;
             this.result_arrrr = sat.arrrr;
           })
           .catch((e) => log('FAILED: ' + e));
    )";
  helper.runner().Run(source, "test.js");

  // Run the message loop to read the request and write the response.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(helper.Get<int>("result_count"), 123);
  EXPECT_EQ(helper.Get<std::string>("result_id"), "here is my id");
  auto result_vector = helper.Get<std::vector<int>>("result_vector");
  ASSERT_EQ(result_vector.size(), 4u);
  EXPECT_EQ(result_vector[0], 1);
  EXPECT_EQ(result_vector[1], -2);
  EXPECT_EQ(result_vector[2], 4);
  EXPECT_EQ(result_vector[3], -8);
  EXPECT_EQ(helper.Get<bool>("result_basic_b"), true);
  EXPECT_EQ(helper.Get<int>("result_basic_i8"), -10);
  EXPECT_EQ(helper.Get<int>("result_basic_i16"), -12);
  EXPECT_EQ(helper.Get<int>("result_basic_i32"), -14);
  EXPECT_EQ(helper.Get<unsigned int>("result_basic_u8"), 16u);
  EXPECT_EQ(helper.Get<unsigned int>("result_basic_u16"), 64000u);
  EXPECT_EQ(helper.Get<unsigned int>("result_basic_u32"), 4000000000u);
  EXPECT_EQ(helper.Get<std::string>("result_later_string"), "ⓣⓔⓡⓜⓘⓝⓐⓣⓞⓡ");
  // Retrieve as a vector as there's no difference in representation in JS (and
  // gin already supports vector), and verify the length matches the expected
  // length of the fidl array.
  auto result_arrrr = helper.Get<std::vector<int32_t>>("result_arrrr");
  ASSERT_EQ(result_arrrr.size(), fidljstest::ARRRR_SIZE);
  for (uint64_t i = 0; i < fidljstest::ARRRR_SIZE; ++i) {
    EXPECT_EQ(result_arrrr[i], static_cast<int32_t>(i * 5) - 10);
  }
}

TEST_F(FidlGenJsTest, HandlePassing) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  TestolaImpl testola_impl;
  fidl::Binding<fidljstest::Testola> binding(&testola_impl);
  binding.Bind(std::move(helper.server()));

  zx::job default_job_copy;
  ASSERT_EQ(zx::job::default_job()->duplicate(ZX_RIGHT_SAME_RIGHTS,
                                              &default_job_copy),
            ZX_OK);
  helper.runner().global()->Set(
      gin::StringToSymbol(isolate, "testJobHandle"),
      gin::ConvertToV8(isolate, default_job_copy.get()));

  // TODO(crbug.com/883496): Handles wrapped in Transferrable once MessagePort
  // is sorted out, and then stop treating handles as unmanaged |uint32_t|s.
  std::string source = R"(
    var proxy = new TestolaProxy();
    proxy.$bind(testHandle);
    proxy.PassHandles(testJobHandle).then(h => {
      this.debuglogHandle = h;
    }).catch((e) => log('FAILED: ' + e));
  )";
  helper.runner().Run(source, "test.js");

  // Run the message loop to send the request and receive a response.
  base::RunLoop().RunUntilIdle();

  zx_handle_t debug_handle_back_from_js =
      helper.Get<uint32_t>("debuglogHandle");
  EXPECT_EQ(debug_handle_back_from_js, testola_impl.unowned_log_handle());

  // Make sure we received the valid handle back correctly, and close it. Not
  // stored into a zx::log in case it isn't valid, and to check the return value
  // from closing it.
  EXPECT_EQ(zx_handle_close(debug_handle_back_from_js), ZX_OK);

  // Ensure we didn't pass away our default job.
  EXPECT_NE(GetKoidForHandle(*zx::job::default_job()), ZX_KOID_INVALID);
}

TEST_F(FidlGenJsTest, UnionSend) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  TestolaImpl testola_impl;
  fidl::Binding<fidljstest::Testola> binding(&testola_impl);
  binding.Bind(std::move(helper.server()));

  std::string source = R"(
    var proxy = new TestolaProxy();
    proxy.$bind(testHandle);
    var somu = new StructOfMultipleUnions();

    var swb = new StructWithBool(/*some_bool*/ true);
    somu.initial.set_swb(swb);

    var lswa = new LargerStructWithArray([]);
    for (var i = 0; i < 32; ++i) {
      lswa.components[i] = i * 99;
    }
    somu.optional.set_lswa(lswa);

    somu.trailing.set_swu(new StructWithUint(123456));

    proxy.ReceiveUnions(somu);
  )";
  helper.runner().Run(source, "test.js");

  base::RunLoop().RunUntilIdle();

  // Expectations on the contents of the union are checked in the body of
  // TestolaImpl::ReceiveAUnion().
  EXPECT_TRUE(testola_impl.did_receive_union());
}

TEST_F(FidlGenJsTest, UnionReceive) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  TestolaImpl testola_impl;
  fidl::Binding<fidljstest::Testola> binding(&testola_impl);
  binding.Bind(std::move(helper.server()));

  std::string source = R"(
    var proxy = new TestolaProxy();
    proxy.$bind(testHandle);
    proxy.SendUnions().then(resp => {
      this.result_initial_is_swb = resp.initial.is_swb();
      this.result_initial_is_swu = resp.initial.is_swu();
      this.result_initial_is_lswa = resp.initial.is_lswa();
      this.result_optional_is_swb = resp.optional.is_swb();
      this.result_optional_is_swu = resp.optional.is_swu();
      this.result_optional_is_lswa = resp.optional.is_lswa();
      this.result_trailing_is_swb = resp.trailing.is_swb();
      this.result_trailing_is_swu = resp.trailing.is_swu();
      this.result_trailing_is_lswa = resp.trailing.is_lswa();

      this.result_initial_some_bool = resp.initial.swb.some_bool;
      this.result_optional_num = resp.optional.swu.num;
    }).catch((e) => log('FAILED: ' + e));
  )";
  helper.runner().Run(source, "test.js");

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(helper.Get<bool>("result_initial_is_swb"));
  EXPECT_FALSE(helper.Get<bool>("result_initial_is_swu"));
  EXPECT_FALSE(helper.Get<bool>("result_initial_is_lswa"));

  EXPECT_FALSE(helper.Get<bool>("result_optional_is_swb"));
  EXPECT_TRUE(helper.Get<bool>("result_optional_is_swu"));
  EXPECT_FALSE(helper.Get<bool>("result_optional_is_lswa"));

  EXPECT_FALSE(helper.Get<bool>("result_trailing_is_swb"));
  EXPECT_FALSE(helper.Get<bool>("result_trailing_is_swu"));
  EXPECT_TRUE(helper.Get<bool>("result_trailing_is_lswa"));

  EXPECT_TRUE(helper.Get<bool>("result_initial_some_bool"));
  EXPECT_EQ(helper.Get<uint32_t>("result_optional_num"), 987654u);
}

TEST_F(FidlGenJsTest, DefaultUsingIdentifier) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  std::string source = R"(
    var temp = new DefaultUsingIdentifier();
    this.result = temp.blorp_defaulting_to_beta;
  )";
  helper.runner().Run(source, "test.js");

  EXPECT_EQ(helper.Get<int>("result"),
            static_cast<int>(fidljstest::Blorp::BETA));
}

TEST_F(FidlGenJsTest, VectorOfStrings) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  TestolaImpl testola_impl;
  fidl::Binding<fidljstest::Testola> binding(&testola_impl);
  binding.Bind(std::move(helper.server()));

  std::string source = R"(
    var proxy = new TestolaProxy();
    proxy.$bind(testHandle);

    var v1 = ['str0', 'str1', 'str2'];
    var v2 = ['str3', null, null, null, 'str4'];
    var v3 = ['0123456789'];  // This is the maximum allowed length.
    proxy.SendVectorsOfString(v1, v2, v3);
  )";
  helper.runner().Run(source, "test.js");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(testola_impl.did_get_vectors_of_string());
}

TEST_F(FidlGenJsTest, VectorOfStringsTooLongString) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  TestolaImpl testola_impl;
  fidl::Binding<fidljstest::Testola> binding(&testola_impl);
  binding.Bind(std::move(helper.server()));

  std::string source = R"(
    var proxy = new TestolaProxy();
    proxy.$bind(testHandle);

    var too_long = ['this string is longer than allowed'];
    proxy.SendVectorsOfString([], [], too_long);
    this.tried_to_send = true;
  )";
  helper.runner().Run(source, "test.js");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(helper.Get<bool>("tried_to_send"));
  EXPECT_FALSE(testola_impl.did_get_vectors_of_string());
}

TEST_F(FidlGenJsTest, VectorOfStruct) {
  v8::Isolate* isolate = instance_->isolate();
  BindingsSetupHelper helper(isolate);

  TestolaImpl testola_impl;
  fidl::Binding<fidljstest::Testola> binding(&testola_impl);
  binding.Bind(std::move(helper.server()));

  std::string source = R"(
    var proxy = new TestolaProxy();
    proxy.$bind(testHandle);

    var data = [
      new StructWithUint(456),
      new StructWithUint(789),
      new StructWithUint(123),
      new StructWithUint(0xfffff),
    ];
    proxy.VectorOfStruct(data).then(resp => {
      this.result_length = resp.length;
      this.result_0 = resp[0].num;
      this.result_1 = resp[1].num;
    }).catch((e) => log('FAILED: ' + e));
  )";
  helper.runner().Run(source, "test.js");
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(helper.Get<uint32_t>("result_length"), 2u);
  EXPECT_EQ(helper.Get<int>("result_0"), 369);
  EXPECT_EQ(helper.Get<int>("result_1"), 258);
}

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
