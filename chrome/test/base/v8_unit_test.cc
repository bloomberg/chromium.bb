// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/v8_unit_test.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/chrome_paths.h"
#include "third_party/WebKit/public/web/WebKit.h"

namespace {

// |args| are passed through the various JavaScript logging functions such as
// console.log. Returns a string appropriate for logging with LOG(severity).
std::string LogArgs2String(const v8::FunctionCallbackInfo<v8::Value>& args) {
  std::string message;
  bool first = true;
  for (int i = 0; i < args.Length(); i++) {
    v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
    if (first)
      first = false;
    else
      message += " ";

    v8::String::Utf8Value str(args[i]);
    message += *str;
  }
  return message;
}

// Whether errors were seen.
bool had_errors = false;

// testDone results.
bool testResult_ok = false;

// Location of test data (currently test/data/webui).
base::FilePath test_data_directory;

// Location of generated test data (<(PROGRAM_DIR)/test_data).
base::FilePath gen_test_data_directory;

}  // namespace

V8UnitTest::V8UnitTest() : handle_scope_(blink::mainThreadIsolate()) {
  InitPathsAndLibraries();
}

V8UnitTest::~V8UnitTest() {
}

void V8UnitTest::AddLibrary(const base::FilePath& library_path) {
  user_libraries_.push_back(library_path);
}

bool V8UnitTest::ExecuteJavascriptLibraries() {
  std::string utf8_content;
  for (std::vector<base::FilePath>::iterator user_libraries_iterator =
           user_libraries_.begin();
       user_libraries_iterator != user_libraries_.end();
       ++user_libraries_iterator) {
    std::string library_content;
    base::FilePath library_file(*user_libraries_iterator);
    if (!user_libraries_iterator->IsAbsolute()) {
      base::FilePath gen_file = gen_test_data_directory.Append(library_file);
      library_file = base::PathExists(gen_file)
                         ? gen_file
                         : test_data_directory.Append(*user_libraries_iterator);
    }
    library_file = base::MakeAbsoluteFilePath(library_file);
    if (!base::ReadFileToString(library_file, &library_content)) {
      ADD_FAILURE() << library_file.value();
      return false;
    }
    ExecuteScriptInContext(library_content, library_file.MaybeAsASCII());
    if (::testing::Test::HasFatalFailure())
      return false;
  }
  return true;
}

bool V8UnitTest::RunJavascriptTestF(const std::string& testFixture,
                                    const std::string& testName) {
  had_errors = false;
  testResult_ok = false;
  std::string test_js;
  if (!ExecuteJavascriptLibraries())
    return false;

  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate, context_);
  v8::Context::Scope context_scope(context);

  v8::Handle<v8::Value> functionProperty =
      context->Global()->Get(v8::String::NewFromUtf8(isolate, "runTest"));
  EXPECT_FALSE(functionProperty.IsEmpty());
  if (::testing::Test::HasNonfatalFailure())
    return false;
  EXPECT_TRUE(functionProperty->IsFunction());
  if (::testing::Test::HasNonfatalFailure())
    return false;
  v8::Handle<v8::Function> function =
      v8::Handle<v8::Function>::Cast(functionProperty);

  v8::Local<v8::Array> params = v8::Array::New(isolate);
  params->Set(0,
              v8::String::NewFromUtf8(isolate,
                                      testFixture.data(),
                                      v8::String::kNormalString,
                                      testFixture.size()));
  params->Set(1,
              v8::String::NewFromUtf8(isolate,
                                      testName.data(),
                                      v8::String::kNormalString,
                                      testName.size()));
  v8::Handle<v8::Value> args[] = {
      v8::Boolean::New(isolate, false),
      v8::String::NewFromUtf8(isolate, "RUN_TEST_F"), params};

  v8::TryCatch try_catch;
  v8::Handle<v8::Value> result = function->Call(context->Global(), 3, args);
  // The test fails if an exception was thrown.
  EXPECT_FALSE(result.IsEmpty());
  if (::testing::Test::HasNonfatalFailure())
    return false;

  // Ok if ran successfully, passed tests, and didn't have console errors.
  return result->BooleanValue() && testResult_ok && !had_errors;
}

void V8UnitTest::InitPathsAndLibraries() {
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory));
  test_data_directory = test_data_directory.AppendASCII("webui");
  ASSERT_TRUE(
      PathService::Get(chrome::DIR_GEN_TEST_DATA, &gen_test_data_directory));

  base::FilePath mockPath;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &mockPath));
  mockPath = mockPath.AppendASCII("chrome");
  mockPath = mockPath.AppendASCII("third_party");
  mockPath = mockPath.AppendASCII("mock4js");
  mockPath = mockPath.AppendASCII("mock4js.js");
  AddLibrary(mockPath);

  base::FilePath accessibilityAuditPath;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &accessibilityAuditPath));
  accessibilityAuditPath = accessibilityAuditPath.AppendASCII("third_party");
  accessibilityAuditPath =
      accessibilityAuditPath.AppendASCII("accessibility-audit");
  accessibilityAuditPath = accessibilityAuditPath.AppendASCII("axs_testing.js");
  AddLibrary(accessibilityAuditPath);

  base::FilePath testApiPath;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &testApiPath));
  testApiPath = testApiPath.AppendASCII("webui");
  testApiPath = testApiPath.AppendASCII("test_api.js");
  AddLibrary(testApiPath);
}

void V8UnitTest::SetUp() {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
  v8::Handle<v8::String> logString = v8::String::NewFromUtf8(isolate, "log");
  v8::Handle<v8::FunctionTemplate> logFunction =
      v8::FunctionTemplate::New(isolate, &V8UnitTest::Log);
  global->Set(logString, logFunction);

  // Set up chrome object for chrome.send().
  v8::Handle<v8::ObjectTemplate> chrome = v8::ObjectTemplate::New(isolate);
  global->Set(v8::String::NewFromUtf8(isolate, "chrome"), chrome);
  chrome->Set(v8::String::NewFromUtf8(isolate, "send"),
              v8::FunctionTemplate::New(isolate, &V8UnitTest::ChromeSend));

  // Set up console object for console.log(), etc.
  v8::Handle<v8::ObjectTemplate> console = v8::ObjectTemplate::New(isolate);
  global->Set(v8::String::NewFromUtf8(isolate, "console"), console);
  console->Set(logString, logFunction);
  console->Set(v8::String::NewFromUtf8(isolate, "info"), logFunction);
  console->Set(v8::String::NewFromUtf8(isolate, "warn"), logFunction);
  console->Set(v8::String::NewFromUtf8(isolate, "error"),
               v8::FunctionTemplate::New(isolate, &V8UnitTest::Error));

  context_.Reset(isolate, v8::Context::New(isolate, NULL, global));
}

void V8UnitTest::SetGlobalStringVar(const std::string& var_name,
                                    const std::string& value) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate, context_);
  v8::Context::Scope context_scope(context);
  context->Global()->Set(
      v8::String::NewFromUtf8(isolate,
                              var_name.c_str(),
                              v8::String::kNormalString,
                              var_name.length()),
      v8::String::NewFromUtf8(
          isolate, value.c_str(), v8::String::kNormalString, value.length()));
}

void V8UnitTest::ExecuteScriptInContext(const base::StringPiece& script_source,
                                        const base::StringPiece& script_name) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate, context_);
  v8::Context::Scope context_scope(context);
  v8::Handle<v8::String> source =
      v8::String::NewFromUtf8(isolate,
                              script_source.data(),
                              v8::String::kNormalString,
                              script_source.size());
  v8::Handle<v8::String> name =
      v8::String::NewFromUtf8(isolate,
                              script_name.data(),
                              v8::String::kNormalString,
                              script_name.size());

  v8::TryCatch try_catch;
  v8::Handle<v8::Script> script = v8::Script::Compile(source, name);
  // Ensure the script compiled without errors.
  if (script.IsEmpty())
    FAIL() << ExceptionToString(try_catch);

  v8::Handle<v8::Value> result = script->Run();
  // Ensure the script ran without errors.
  if (result.IsEmpty())
    FAIL() << ExceptionToString(try_catch);
}

std::string V8UnitTest::ExceptionToString(const v8::TryCatch& try_catch) {
  std::string str;
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  v8::String::Utf8Value exception(try_catch.Exception());
  v8::Local<v8::Message> message(try_catch.Message());
  if (message.IsEmpty()) {
    str.append(base::StringPrintf("%s\n", *exception));
  } else {
    v8::String::Utf8Value filename(message->GetScriptOrigin().ResourceName());
    int linenum = message->GetLineNumber();
    int colnum = message->GetStartColumn();
    str.append(base::StringPrintf(
        "%s:%i:%i %s\n", *filename, linenum, colnum, *exception));
    v8::String::Utf8Value sourceline(message->GetSourceLine());
    str.append(base::StringPrintf("%s\n", *sourceline));
  }
  return str;
}

void V8UnitTest::TestFunction(const std::string& function_name) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate, context_);
  v8::Context::Scope context_scope(context);

  v8::Handle<v8::Value> functionProperty = context->Global()->Get(
      v8::String::NewFromUtf8(isolate, function_name.c_str()));
  ASSERT_FALSE(functionProperty.IsEmpty());
  ASSERT_TRUE(functionProperty->IsFunction());
  v8::Handle<v8::Function> function =
      v8::Handle<v8::Function>::Cast(functionProperty);

  v8::TryCatch try_catch;
  v8::Handle<v8::Value> result = function->Call(context->Global(), 0, NULL);
  // The test fails if an exception was thrown.
  if (result.IsEmpty())
    FAIL() << ExceptionToString(try_catch);
}

// static
void V8UnitTest::Log(const v8::FunctionCallbackInfo<v8::Value>& args) {
  LOG(INFO) << LogArgs2String(args);
}

void V8UnitTest::Error(const v8::FunctionCallbackInfo<v8::Value>& args) {
  had_errors = true;
  LOG(ERROR) << LogArgs2String(args);
}

void V8UnitTest::ChromeSend(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  // We expect to receive 2 args: ("testResult", [ok, message]). However,
  // chrome.send may pass only one. Therefore we need to ensure we have at least
  // 1, then ensure that the first is "testResult" before checking again for 2.
  EXPECT_LE(1, args.Length());
  if (::testing::Test::HasNonfatalFailure())
    return;
  v8::String::Utf8Value message(args[0]);
  EXPECT_EQ("testResult", std::string(*message, message.length()));
  if (::testing::Test::HasNonfatalFailure())
    return;
  EXPECT_EQ(2, args.Length());
  if (::testing::Test::HasNonfatalFailure())
    return;
  v8::Handle<v8::Array> testResult(args[1].As<v8::Array>());
  EXPECT_EQ(2U, testResult->Length());
  if (::testing::Test::HasNonfatalFailure())
    return;
  testResult_ok = testResult->Get(0)->BooleanValue();
  if (!testResult_ok) {
    v8::String::Utf8Value message(testResult->Get(1));
    LOG(ERROR) << *message;
  }
}
