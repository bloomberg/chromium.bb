// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/parsers/target/parser_impl.h"

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/values.h"
#include "chrome/chrome_cleaner/interfaces/parser_interface.mojom.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/parsers/json_parser/sandboxed_json_parser.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::WaitableEvent;

namespace chrome_cleaner {

namespace {

const char kTestJsonKey[] = "name";
const char kTestJsonValue[] = "Jason";
const char kTestJsonText[] = "{ \"name\": \"Jason\" }";
const char kInvalidJsonText[] = "{ name: jason }";

class ParserImplTest : public testing::Test {
 public:
  ParserImplTest()
      : task_runner_(MojoTaskRunner::Create()),
        parser_ptr_(new mojom::ParserPtr(),
                    base::OnTaskRunnerDeleter(task_runner_)),
        parser_impl_(nullptr, base::OnTaskRunnerDeleter(task_runner_)),
        sandboxed_json_parser_(task_runner_.get(), parser_ptr_.get()) {
    BindParser();
  }

 protected:
  void BindParser() {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](mojom::ParserPtr* parser,
               std::unique_ptr<ParserImpl, base::OnTaskRunnerDeleter>*
                   parser_impl) {
              parser_impl->reset(
                  new ParserImpl(mojo::MakeRequest(parser), base::DoNothing()));
            },
            parser_ptr_.get(), &parser_impl_));
  }

  scoped_refptr<MojoTaskRunner> task_runner_;
  std::unique_ptr<mojom::ParserPtr, base::OnTaskRunnerDeleter> parser_ptr_;
  std::unique_ptr<ParserImpl, base::OnTaskRunnerDeleter> parser_impl_;
  SandboxedJsonParser sandboxed_json_parser_;
};

}  // namespace

TEST_F(ParserImplTest, ParseJson) {
  WaitableEvent done(WaitableEvent::ResetPolicy::MANUAL,
                     WaitableEvent::InitialState::NOT_SIGNALED);
  sandboxed_json_parser_.Parse(
      kTestJsonText,
      base::BindOnce(
          [](WaitableEvent* done, base::Optional<base::Value> value,
             const base::Optional<std::string>& error) {
            ASSERT_FALSE(error.has_value());
            ASSERT_TRUE(value.has_value());
            ASSERT_TRUE(value->is_dict());
            const base::DictionaryValue* dict;
            ASSERT_TRUE(value->GetAsDictionary(&dict));

            std::string string_value;
            ASSERT_TRUE(dict->GetString(kTestJsonKey, &string_value));
            EXPECT_EQ(kTestJsonValue, string_value);
            done->Signal();
          },
          &done));
  EXPECT_TRUE(done.TimedWait(TestTimeouts::action_timeout()));
}

TEST_F(ParserImplTest, ParseJsonError) {
  WaitableEvent done(WaitableEvent::ResetPolicy::MANUAL,
                     WaitableEvent::InitialState::NOT_SIGNALED);
  sandboxed_json_parser_.Parse(
      kInvalidJsonText,
      base::BindOnce(
          [](WaitableEvent* done, base::Optional<base::Value> value,
             const base::Optional<std::string>& error) {
            ASSERT_TRUE(error.has_value());
            EXPECT_FALSE(error->empty());
            done->Signal();
          },
          &done));
  EXPECT_TRUE(done.TimedWait(TestTimeouts::action_timeout()));
}

}  // namespace chrome_cleaner
