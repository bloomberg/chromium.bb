// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/dwrite_font_proxy_message_filter_win.h"

#include <dwrite.h>

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "content/common/dwrite_font_proxy_messages.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "ipc/ipc_message_macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/win/direct_write.h"

namespace mswr = Microsoft::WRL;

namespace content {

namespace {

class FilterWithFakeSender : public DWriteFontProxyMessageFilter {
 public:
  bool Send(IPC::Message* msg) override {
    EXPECT_EQ(nullptr, reply_message_.get());
    reply_message_.reset(msg);
    return true;
  }

  IPC::Message* GetReply() { return reply_message_.get(); }

  void ResetReply() { reply_message_.reset(nullptr); }

 private:
  ~FilterWithFakeSender() override = default;

  scoped_ptr<IPC::Message> reply_message_;
};

class DWriteFontProxyMessageFilterUnitTest : public testing::Test {
 public:
  DWriteFontProxyMessageFilterUnitTest() {
    filter_ = new FilterWithFakeSender();
  }

  void Send(IPC::SyncMessage* message) {
    std::unique_ptr<IPC::SyncMessage> deleter(message);
    scoped_ptr<IPC::MessageReplyDeserializer> serializer(
        message->GetReplyDeserializer());
    filter_->OnMessageReceived(*message);
    base::RunLoop().RunUntilIdle();
    ASSERT_NE(nullptr, filter_->GetReply());
    serializer->SerializeOutputParameters(*(filter_->GetReply()));
  }

  scoped_refptr<FilterWithFakeSender> filter_;
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(DWriteFontProxyMessageFilterUnitTest, GetFamilyCount) {
  if (!gfx::win::ShouldUseDirectWrite())
    return;
  UINT32 family_count = 0;
  Send(new DWriteFontProxyMsg_GetFamilyCount(&family_count));
  EXPECT_NE(0u, family_count);  // Assume there's some fonts on the test system.
}

TEST_F(DWriteFontProxyMessageFilterUnitTest, FindFamily) {
  if (!gfx::win::ShouldUseDirectWrite())
    return;
  UINT32 arial_index = 0;
  Send(new DWriteFontProxyMsg_FindFamily(L"Arial", &arial_index));
  EXPECT_NE(UINT_MAX, arial_index);

  filter_->ResetReply();
  UINT32 times_index = 0;
  Send(new DWriteFontProxyMsg_FindFamily(L"Times New Roman", &times_index));
  EXPECT_NE(UINT_MAX, times_index);
  EXPECT_NE(arial_index, times_index);

  filter_->ResetReply();
  UINT32 unknown_index = 0;
  Send(new DWriteFontProxyMsg_FindFamily(L"Not a font family", &unknown_index));
  EXPECT_EQ(UINT_MAX, unknown_index);
}

TEST_F(DWriteFontProxyMessageFilterUnitTest, GetFamilyNames) {
  if (!gfx::win::ShouldUseDirectWrite())
    return;
  UINT32 arial_index = 0;
  Send(new DWriteFontProxyMsg_FindFamily(L"Arial", &arial_index));
  filter_->ResetReply();

  std::vector<DWriteStringPair> names;
  Send(new DWriteFontProxyMsg_GetFamilyNames(arial_index, &names));

  EXPECT_LT(0u, names.size());
  for (const auto& pair : names) {
    EXPECT_STRNE(L"", pair.first.c_str());
    EXPECT_STRNE(L"", pair.second.c_str());
  }
}

TEST_F(DWriteFontProxyMessageFilterUnitTest, GetFamilyNamesIndexOutOfBounds) {
  if (!gfx::win::ShouldUseDirectWrite())
    return;
  std::vector<DWriteStringPair> names;
  UINT32 invalid_index = 1000000;
  Send(new DWriteFontProxyMsg_GetFamilyNames(invalid_index, &names));

  EXPECT_EQ(0u, names.size());
}

TEST_F(DWriteFontProxyMessageFilterUnitTest, GetFontFiles) {
  if (!gfx::win::ShouldUseDirectWrite())
    return;
  UINT32 arial_index = 0;
  Send(new DWriteFontProxyMsg_FindFamily(L"Arial", &arial_index));
  filter_->ResetReply();

  std::vector<base::string16> files;
  Send(new DWriteFontProxyMsg_GetFontFiles(arial_index, &files));

  EXPECT_LT(0u, files.size());
  for (const base::string16& file : files) {
    EXPECT_STRNE(L"", file.c_str());
  }
}

TEST_F(DWriteFontProxyMessageFilterUnitTest, GetFontFilesIndexOutOfBounds) {
  if (!gfx::win::ShouldUseDirectWrite())
    return;
  std::vector<base::string16> files;
  UINT32 invalid_index = 1000000;
  Send(new DWriteFontProxyMsg_GetFontFiles(invalid_index, &files));

  EXPECT_EQ(0u, files.size());
}

}  // namespace

}  // namespace content
