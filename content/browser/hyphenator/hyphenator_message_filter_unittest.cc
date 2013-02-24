// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/hyphenator/hyphenator_message_filter.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "content/common/hyphenator_messages.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_platform_file.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

// A class derived from the HyphenatorMessageFilter class used in unit tests.
// This class overrides some methods so we can test the HyphenatorMessageFilter
// class without posting tasks.
class TestHyphenatorMessageFilter : public HyphenatorMessageFilter {
 public:
  explicit TestHyphenatorMessageFilter(RenderProcessHost* host)
      : HyphenatorMessageFilter(host),
        type_(0),
        file_(base::kInvalidPlatformFileValue) {
  }

  const string16& locale() const { return locale_; }
  uint32 type() const { return type_; }
  base::PlatformFile file() const { return file_; }

  // BrowserMessageFilter implementation.
  virtual bool Send(IPC::Message* message) OVERRIDE {
    if (message->type() != HyphenatorMsg_SetDictionary::ID)
      return false;

    // Read the PlatformFileForTransit object and check if its value is
    // kInvalidPlatformFileValue. Close the incoming file if it is not
    // kInvalidPlatformFileValue to prevent leaving the dictionary file open.
    type_ = message->type();
    PickleIterator iter(*message);
    IPC::PlatformFileForTransit file;
    IPC::ParamTraits<IPC::PlatformFileForTransit>::Read(message, &iter, &file);
    file_ = IPC::PlatformFileForTransitToPlatformFile(file);
    delete message;
    return true;
  }

  void SetDictionary(base::PlatformFile file) {
    dictionary_file_ = file;
  }

  void Reset() {
    if (dictionary_file_ != base::kInvalidPlatformFileValue) {
      base::ClosePlatformFile(dictionary_file_);
      dictionary_file_ = base::kInvalidPlatformFileValue;
    }
    locale_.clear();
    type_ = 0;
    if (file_ != base::kInvalidPlatformFileValue) {
      base::ClosePlatformFile(file_);
      file_ = base::kInvalidPlatformFileValue;
    }
  }

 private:
  virtual ~TestHyphenatorMessageFilter() {
  }

  // HyphenatorMessageFilter implementation. This function emulates the
  // original implementation without posting a task.
  virtual void OnOpenDictionary(const string16& locale) OVERRIDE {
    locale_ = locale;
    if (dictionary_file_ == base::kInvalidPlatformFileValue)
      OpenDictionary(locale);
    SendDictionary();
  }

  string16 locale_;
  uint32 type_;
  base::PlatformFile file_;
};

class HyphenatorMessageFilterTest : public testing::Test {
 public:
  HyphenatorMessageFilterTest() {
    context_.reset(new TestBrowserContext);
    host_.reset(new MockRenderProcessHost(context_.get()));
    filter_ = new TestHyphenatorMessageFilter(host_.get());
  }

  virtual ~HyphenatorMessageFilterTest() {}

  scoped_ptr<TestBrowserContext> context_;
  scoped_ptr<MockRenderProcessHost> host_;
  scoped_refptr<TestHyphenatorMessageFilter> filter_;
};

// Verifies IPC messages sent by the HyphenatorMessageFilter class when it
// receives IPC messages (HyphenatorHostMsg_OpenDictionary).
TEST_F(HyphenatorMessageFilterTest, OpenDictionary) {
  // Send a HyphenatorHostMsg_OpenDictionary message with an invalid locale and
  // verify it sends a HyphenatorMsg_SetDictionary message with an invalid file.
  string16 invalid_locale(ASCIIToUTF16("xx-xx"));
  IPC::Message invalid_message(
      0, HyphenatorHostMsg_OpenDictionary::ID, IPC::Message::PRIORITY_NORMAL);
  invalid_message.WriteString16(invalid_locale);

  bool message_was_ok = false;
  filter_->OnMessageReceived(invalid_message, &message_was_ok);
  EXPECT_TRUE(message_was_ok);
  EXPECT_EQ(invalid_locale, filter_->locale());
  EXPECT_EQ(HyphenatorMsg_SetDictionary::ID, filter_->type());
  EXPECT_EQ(base::kInvalidPlatformFileValue, filter_->file());

  filter_->Reset();

  // Open a sample dictionary file and attach it to the
  // HyphenatorMessageFilter class so it can return a valid file.
  base::FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.Append(FILE_PATH_LITERAL("third_party"));
  path = path.Append(FILE_PATH_LITERAL("hyphen"));
  path = path.Append(FILE_PATH_LITERAL("hyph_en_US.dic"));
  base::PlatformFile file = base::CreatePlatformFile(
      path, base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ,
      NULL, NULL);
  EXPECT_NE(base::kInvalidPlatformFileValue, file);
  filter_->SetDictionary(file);
  file = base::kInvalidPlatformFileValue;  // Ownership has been transferred.

  // Send a HyphenatorHostMsg_OpenDictionary message with an empty locale and
  // verify it sends a HyphenatorMsg_SetDictionary message with a valid file.
  string16 empty_locale;
  IPC::Message valid_message(
      0, HyphenatorHostMsg_OpenDictionary::ID, IPC::Message::PRIORITY_NORMAL);
  valid_message.WriteString16(empty_locale);

  message_was_ok = false;
  filter_->OnMessageReceived(valid_message, &message_was_ok);
  EXPECT_TRUE(message_was_ok);
  EXPECT_EQ(empty_locale, filter_->locale());
  EXPECT_EQ(HyphenatorMsg_SetDictionary::ID, filter_->type());
  EXPECT_NE(base::kInvalidPlatformFileValue, filter_->file());

  // Delete all resources used by this test.
  filter_->Reset();
}

}  // namespace content
