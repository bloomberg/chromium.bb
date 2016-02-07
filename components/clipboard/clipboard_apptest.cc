// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "components/clipboard/public/interfaces/clipboard.mojom.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/shell/public/cpp/application_test_base.h"
#include "mojo/shell/public/cpp/shell.h"

using mojo::Array;
using mojo::Clipboard;
using mojo::Map;
using mojo::String;

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
                             const Array<uint8_t>& input) {
  *string_is_null = input.is_null();
  *output = input.is_null() ? "" : input.To<std::string>();
  run_loop->Quit();
}

void CopyVectorStringAndEndRunloop(std::vector<std::string>* output,
                                   base::RunLoop* run_loop,
                                   const Array<String>& input) {
  *output = input.To<std::vector<std::string> >();
  run_loop->Quit();
}

const char* kUninitialized = "Uninitialized data";
const char* kPlainTextData = "Some plain data";
const char* kHtmlData = "<html>data</html>";

}  // namespace

namespace clipboard {

class ClipboardAppTest : public mojo::test::ApplicationTestBase {
 public:
  ClipboardAppTest() : ApplicationTestBase() {}
  ~ClipboardAppTest() override {}

  void SetUp() override {
    mojo::test::ApplicationTestBase::SetUp();
    shell()->ConnectToService("mojo:clipboard", &clipboard_);
  }

  uint64_t GetSequenceNumber() {
    base::RunLoop run_loop;
    uint64_t sequence_num = 999999;
    clipboard_->GetSequenceNumber(
        Clipboard::Type::COPY_PASTE,
        base::Bind(&CopyUint64AndEndRunloop, &sequence_num, &run_loop));
    run_loop.Run();
    return sequence_num;
  }

  std::vector<std::string> GetAvailableFormatMimeTypes() {
    base::RunLoop run_loop;
    std::vector<std::string> types;
    types.push_back(kUninitialized);
    clipboard_->GetAvailableMimeTypes(
        Clipboard::Type::COPY_PASTE,
        base::Bind(&CopyVectorStringAndEndRunloop, &types, &run_loop));
    run_loop.Run();
    return types;
  }

  bool GetDataOfType(const std::string& mime_type, std::string* data) {
    base::RunLoop run_loop;
    bool is_null = false;
    clipboard_->ReadMimeType(
        Clipboard::Type::COPY_PASTE, mime_type,
        base::Bind(&CopyStringAndEndRunloop, data, &is_null, &run_loop));
    run_loop.Run();
    return !is_null;
  }

  void SetStringText(const std::string& data) {
    Map<String, Array<uint8_t>> mime_data;
    mime_data[Clipboard::MIME_TYPE_TEXT] = Array<uint8_t>::From(data);
    clipboard_->WriteClipboardData(Clipboard::Type::COPY_PASTE,
                                   std::move(mime_data));
  }

 protected:
  mojo::ClipboardPtr clipboard_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardAppTest);
};

TEST_F(ClipboardAppTest, EmptyClipboardOK) {
  EXPECT_EQ(0ul, GetSequenceNumber());
  EXPECT_TRUE(GetAvailableFormatMimeTypes().empty());
  std::string data;
  EXPECT_FALSE(GetDataOfType(Clipboard::MIME_TYPE_TEXT, &data));
}

TEST_F(ClipboardAppTest, CanReadBackText) {
  std::string data;
  EXPECT_FALSE(GetDataOfType(Clipboard::MIME_TYPE_TEXT, &data));
  EXPECT_EQ(0ul, GetSequenceNumber());

  SetStringText(kPlainTextData);
  EXPECT_EQ(1ul, GetSequenceNumber());

  EXPECT_TRUE(GetDataOfType(Clipboard::MIME_TYPE_TEXT, &data));
  EXPECT_EQ(kPlainTextData, data);
}

TEST_F(ClipboardAppTest, CanSetMultipleDataTypesAtOnce) {
  Map<String, Array<uint8_t>> mime_data;
  mime_data[Clipboard::MIME_TYPE_TEXT] =
      Array<uint8_t>::From(std::string(kPlainTextData));
  mime_data[Clipboard::MIME_TYPE_HTML] =
      Array<uint8_t>::From(std::string(kHtmlData));

  clipboard_->WriteClipboardData(Clipboard::Type::COPY_PASTE,
                                 std::move(mime_data));

  EXPECT_EQ(1ul, GetSequenceNumber());

  std::string data;
  EXPECT_TRUE(GetDataOfType(Clipboard::MIME_TYPE_TEXT, &data));
  EXPECT_EQ(kPlainTextData, data);
  EXPECT_TRUE(GetDataOfType(Clipboard::MIME_TYPE_HTML, &data));
  EXPECT_EQ(kHtmlData, data);
}

TEST_F(ClipboardAppTest, CanClearClipboardWithZeroArray) {
  std::string data;
  SetStringText(kPlainTextData);
  EXPECT_EQ(1ul, GetSequenceNumber());

  EXPECT_TRUE(GetDataOfType(Clipboard::MIME_TYPE_TEXT, &data));
  EXPECT_EQ(kPlainTextData, data);

  Map<String, Array<uint8_t>> mime_data;
  clipboard_->WriteClipboardData(Clipboard::Type::COPY_PASTE,
                                 std::move(mime_data));

  EXPECT_EQ(2ul, GetSequenceNumber());
  EXPECT_FALSE(GetDataOfType(Clipboard::MIME_TYPE_TEXT, &data));
}

}  // namespace clipboard
