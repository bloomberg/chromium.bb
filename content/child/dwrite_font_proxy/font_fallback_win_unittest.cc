// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/dwrite_font_proxy/font_fallback_win.h"

#include <dwrite.h>
#include <shlobj.h>
#include <wrl.h>

#include <vector>

#include "base/memory/ref_counted.h"
#include "content/child/dwrite_font_proxy/dwrite_font_proxy_win.h"
#include "content/common/dwrite_text_analysis_source_win.h"
#include "content/test/dwrite_font_fake_sender_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mswr = Microsoft::WRL;

namespace content {

namespace {

class FontFallbackUnitTest : public testing::Test {
 public:
  FontFallbackUnitTest() {
    CreateDWriteFactory(&factory_);

    factory_->CreateNumberSubstitution(DWRITE_NUMBER_SUBSTITUTION_METHOD_NONE,
                                       L"en-us", true /* ignoreUserOverride */,
                                       &number_substitution_);

    std::vector<base::char16> font_path;
    font_path.resize(MAX_PATH);
    SHGetSpecialFolderPath(nullptr /* hwndOwner - reserved */, font_path.data(),
                           CSIDL_FONTS, FALSE /* fCreate*/);
    base::string16 arial_path;
    arial_path.append(font_path.data()).append(L"\\arial.ttf");

    fake_collection_ = new FakeFontCollection();
    fake_collection_->AddFont(L"Arial")
        .AddFamilyName(L"en-us", L"Arial")
        .AddFilePath(arial_path);

    mswr::MakeAndInitialize<DWriteFontCollectionProxy>(
        &collection_, factory_.Get(), fake_collection_->GetSender());
  }

  void CreateDWriteFactory(IUnknown** factory) {
    using DWriteCreateFactoryProc = decltype(DWriteCreateFactory)*;
    HMODULE dwrite_dll = LoadLibraryW(L"dwrite.dll");
    if (!dwrite_dll)
      return;

    DWriteCreateFactoryProc dwrite_create_factory_proc =
        reinterpret_cast<DWriteCreateFactoryProc>(
            GetProcAddress(dwrite_dll, "DWriteCreateFactory"));
    if (!dwrite_create_factory_proc)
      return;

    dwrite_create_factory_proc(DWRITE_FACTORY_TYPE_SHARED,
                               __uuidof(IDWriteFactory), factory);
  }

  scoped_refptr<FakeFontCollection> fake_collection_;
  mswr::ComPtr<IDWriteFactory> factory_;
  mswr::ComPtr<DWriteFontCollectionProxy> collection_;
  mswr::ComPtr<IDWriteNumberSubstitution> number_substitution_;
};

TEST_F(FontFallbackUnitTest, MapCharacters) {
  mswr::ComPtr<FontFallback> fallback;
  mswr::MakeAndInitialize<FontFallback>(&fallback, collection_.Get(),
                                        fake_collection_->GetTrackingSender());

  mswr::ComPtr<IDWriteFont> font;
  UINT32 mapped_length = 0;
  float scale = 0.0;

  mswr::ComPtr<TextAnalysisSource> text;
  mswr::MakeAndInitialize<TextAnalysisSource>(
      &text, L"hello", L"en-us", number_substitution_.Get(),
      DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
  fallback->MapCharacters(text.Get(), 0, 1, nullptr, nullptr,
                          DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                          DWRITE_FONT_STRETCH_NORMAL, &mapped_length, &font,
                          &scale);

  EXPECT_EQ(1u, mapped_length);  // The fake sender only maps one character
  EXPECT_NE(nullptr, font.Get());
}

}  // namespace
}  // namespace content
