// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/macros.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/services/public/interfaces/clipboard/clipboard.mojom.h"
#include "mojo/shell/shell_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void CopyUint64AndEndRunloop(uint64_t* output,
                             base::RunLoop* run_loop,
                             uint64_t input) {
  *output = input;
  run_loop->Quit();
}

void CopyStringAndEndRunloop(std::string* output,
                             bool* string_is_null,
                             base::RunLoop* run_loop,
                             const mojo::Array<uint8_t>& input) {
  *string_is_null = input.is_null();
  *output = input.is_null() ? "" : input.To<std::string>();
  run_loop->Quit();
}

void CopyVectorStringAndEndRunloop(std::vector<std::string>* output,
                                   base::RunLoop* run_loop,
                                   const mojo::Array<mojo::String>& input) {
  *output = input.To<std::vector<std::string> >();
  run_loop->Quit();
}

const char* kUninitialized = "Uninitialized data";
const char* kPlainTextData = "Some plain data";
const char* kHtmlData = "<html>data</html>";

}  // namespace

namespace mojo {
namespace service {

class ClipboardStandaloneTest : public testing::Test {
 public:
  ClipboardStandaloneTest() {}
  virtual ~ClipboardStandaloneTest() {}

  virtual void SetUp() override {
    test_helper_.Init();

    test_helper_.application_manager()->ConnectToService(GURL("mojo:clipboard"),
                                                         &clipboard_);
  }

  uint64_t GetSequenceNumber() {
    base::RunLoop run_loop;
    uint64_t sequence_num = 999999;
    clipboard_->GetSequenceNumber(
        mojo::Clipboard::TYPE_COPY_PASTE,
        base::Bind(&CopyUint64AndEndRunloop, &sequence_num, &run_loop));
    run_loop.Run();
    return sequence_num;
  }

  std::vector<std::string> GetAvailableFormatMimeTypes() {
    base::RunLoop run_loop;
    std::vector<std::string> types;
    types.push_back(kUninitialized);
    clipboard_->GetAvailableMimeTypes(
        mojo::Clipboard::TYPE_COPY_PASTE,
        base::Bind(&CopyVectorStringAndEndRunloop, &types, &run_loop));
    run_loop.Run();
    return types;
  }

  bool GetDataOfType(const std::string& mime_type, std::string* data) {
    base::RunLoop run_loop;
    bool is_null = false;
    clipboard_->ReadMimeType(
        mojo::Clipboard::TYPE_COPY_PASTE,
        mime_type,
        base::Bind(&CopyStringAndEndRunloop, data, &is_null, &run_loop));
    run_loop.Run();
    return !is_null;
  }

  void SetStringText(const std::string& data) {
    Map<String, Array<uint8_t>> mime_data;
    mime_data[mojo::Clipboard::MIME_TYPE_TEXT] = Array<uint8_t>::From(data);
    clipboard_->WriteClipboardData(mojo::Clipboard::TYPE_COPY_PASTE,
                                   mime_data.Pass());
  }

 protected:
  base::ShadowingAtExitManager at_exit_;
  shell::ShellTestHelper test_helper_;

  ClipboardPtr clipboard_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardStandaloneTest);
};

TEST_F(ClipboardStandaloneTest, EmptyClipboardOK) {
  EXPECT_EQ(0ul, GetSequenceNumber());
  EXPECT_TRUE(GetAvailableFormatMimeTypes().empty());
  std::string data;
  EXPECT_FALSE(GetDataOfType(mojo::Clipboard::MIME_TYPE_TEXT, &data));
}

TEST_F(ClipboardStandaloneTest, CanReadBackText) {
  std::string data;
  EXPECT_FALSE(GetDataOfType(mojo::Clipboard::MIME_TYPE_TEXT, &data));
  EXPECT_EQ(0ul, GetSequenceNumber());

  SetStringText(kPlainTextData);
  EXPECT_EQ(1ul, GetSequenceNumber());

  EXPECT_TRUE(GetDataOfType(mojo::Clipboard::MIME_TYPE_TEXT, &data));
  EXPECT_EQ(kPlainTextData, data);
}

TEST_F(ClipboardStandaloneTest, CanSetMultipleDataTypesAtOnce) {
  Map<String, Array<uint8_t>> mime_data;
  mime_data[mojo::Clipboard::MIME_TYPE_TEXT] =
      Array<uint8_t>::From(std::string(kPlainTextData));
  mime_data[mojo::Clipboard::MIME_TYPE_HTML] =
      Array<uint8_t>::From(std::string(kHtmlData));

  clipboard_->WriteClipboardData(mojo::Clipboard::TYPE_COPY_PASTE,
                                 mime_data.Pass());

  EXPECT_EQ(1ul, GetSequenceNumber());

  std::string data;
  EXPECT_TRUE(GetDataOfType(mojo::Clipboard::MIME_TYPE_TEXT, &data));
  EXPECT_EQ(kPlainTextData, data);
  EXPECT_TRUE(GetDataOfType(mojo::Clipboard::MIME_TYPE_HTML, &data));
  EXPECT_EQ(kHtmlData, data);
}

TEST_F(ClipboardStandaloneTest, CanClearClipboardWithZeroArray) {
  std::string data;
  SetStringText(kPlainTextData);
  EXPECT_EQ(1ul, GetSequenceNumber());

  EXPECT_TRUE(GetDataOfType(mojo::Clipboard::MIME_TYPE_TEXT, &data));
  EXPECT_EQ(kPlainTextData, data);

  Map<String, Array<uint8_t>> mime_data;
  clipboard_->WriteClipboardData(mojo::Clipboard::TYPE_COPY_PASTE,
                                 mime_data.Pass());

  EXPECT_EQ(2ul, GetSequenceNumber());
  EXPECT_FALSE(GetDataOfType(mojo::Clipboard::MIME_TYPE_TEXT, &data));
}

}  // namespace service
}  // namespace mojo
