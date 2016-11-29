// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "blimp/engine/browser/font_data_fetcher.h"
#include "blimp/engine/mojo/font_fetcher_mojo_impl.h"
#include "mojo/public/cpp/system/buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace engine {
namespace {

const char kFakeHash[] = "fake hash";
const char kTestString[] = "Hello world";

class FakeFontDataFetcher : public FontDataFetcher {
 public:
  FakeFontDataFetcher() {}

  void FetchFontStream(const std::string& font_hash,
                       const FontResponseCallback& callback) const override {
    // Return an empty stream if font_hash is empty, otherwise return a SkStream
    // which contains kTestString.
    if (font_hash.empty()) {
      callback.Run(base::MakeUnique<SkMemoryStream>());
    } else {
      callback.Run(base::MakeUnique<SkMemoryStream>(
          kTestString, arraysize(kTestString) - 1));
    }
  }
};

void VerifyEmptyDataWrite(mojo::ScopedSharedBufferHandle handle,
                              uint32_t size) {
  ASSERT_FALSE(handle.is_valid());
  EXPECT_EQ(size, (uint32_t)0);
}

void VerifyDataWriteCorrectly(mojo::ScopedSharedBufferHandle handle,
                              uint32_t size) {
  ASSERT_TRUE(handle.is_valid());

  mojo::ScopedSharedBufferMapping mapping = handle->Map(size);
  ASSERT_TRUE(mapping);

  std::string contents(static_cast<const char*>(mapping.get()), size);
  EXPECT_EQ(kTestString, contents);
}

class FontFetcherMojoImplUnittest : public testing::Test {
 public:
  FontFetcherMojoImplUnittest()
      : font_fetcher_mojo_impl_(new FakeFontDataFetcher()) {}
  ~FontFetcherMojoImplUnittest() override {}

 protected:
  base::MessageLoop message_loop_;
  FontFetcherMojoImpl font_fetcher_mojo_impl_;
};

TEST_F(FontFetcherMojoImplUnittest, VerifyWriteToDataPipe) {
  mojom::FontFetcherPtr mojo_ptr;
  font_fetcher_mojo_impl_.BindRequest(GetProxy(&mojo_ptr));

  // Expect that dummy font data is written correctly to a Mojo DataPipe.
  mojo_ptr->GetFontStream(kFakeHash, base::Bind(&VerifyDataWriteCorrectly));

  // Expect empty handle if the font stream is empty.
  mojo_ptr->GetFontStream("", base::Bind(&VerifyEmptyDataWrite));
}

}  // namespace
}  // namespace engine
}  // namespace blimp
