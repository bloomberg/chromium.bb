// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/handle_table.h"

#include <memory>

#include "base/synchronization/lock.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event_argument.h"
#include "mojo/edk/system/dispatcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace edk {
namespace {

class FakeMessagePipeDispatcher : public Dispatcher {
 public:
  FakeMessagePipeDispatcher() {}

  Type GetType() const override { return Type::MESSAGE_PIPE; }

  MojoResult Close() override { return MOJO_RESULT_OK; }

 private:
  ~FakeMessagePipeDispatcher() override {}
  DISALLOW_COPY_AND_ASSIGN(FakeMessagePipeDispatcher);
};

void CheckNameAndValue(base::trace_event::ProcessMemoryDump* pmd,
                       const std::string& name,
                       const std::string& value) {
  base::trace_event::MemoryAllocatorDump* mad = pmd->GetAllocatorDump(name);
  ASSERT_TRUE(mad);

  std::unique_ptr<base::Value> dict_value =
      mad->attributes_for_testing()->ToBaseValue();
  ASSERT_TRUE(dict_value->is_dict());

  base::DictionaryValue* dict;
  ASSERT_TRUE(dict_value->GetAsDictionary(&dict));

  base::DictionaryValue* inner_dict;
  ASSERT_TRUE(dict->GetDictionary("object_count", &inner_dict));

  std::string output;
  ASSERT_TRUE(inner_dict->GetString("value", &output));
  EXPECT_EQ(value, output);
}

}  // namespace

TEST(HandleTableTest, OnMemoryDump) {
  HandleTable ht;

  {
    base::AutoLock auto_lock(ht.GetLock());
    scoped_refptr<mojo::edk::Dispatcher> dispatcher(
        new FakeMessagePipeDispatcher);
    ht.AddDispatcher(dispatcher);
  }

  base::trace_event::MemoryDumpArgs args;
  base::trace_event::ProcessMemoryDump pmd(nullptr, args);
  ht.OnMemoryDump(args, &pmd);

  CheckNameAndValue(&pmd, "mojo/message_pipe", "1");
  CheckNameAndValue(&pmd, "mojo/data_pipe_consumer", "0");
}

}  // namespace edk
}  // namespace mojo
