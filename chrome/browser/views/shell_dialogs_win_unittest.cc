// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_dialogs.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(AppendExtensionIfNeeded, EndingInPeriod_ExtensionAppended) {
  const std::wstring filename = L"sample.txt.";
  const std::wstring filter_selected = L"*.txt";
  const std::wstring suggested_ext = L"txt";

  const std::wstring actual_filename = AppendExtensionIfNeeded(filename,
      filter_selected, suggested_ext);

  ASSERT_EQ(L"sample.txt.txt", actual_filename);
}

TEST(AppendExtensionIfNeeded, UnknownMimeType_ExtensionAppended) {
  const std::wstring filename = L"sample.unknown-mime-type";
  const std::wstring filter_selected = L"*.txt";
  const std::wstring suggested_ext = L"txt";

  const std::wstring actual_filename = AppendExtensionIfNeeded(filename,
      filter_selected, suggested_ext);

  ASSERT_EQ(L"sample.unknown-mime-type.txt", actual_filename);
}

TEST(AppendExtensionIfNeeded, RecognizableMimeType_NoExtensionAppended) {
  const std::wstring filename = L"sample.html";
  const std::wstring filter_selected = L"*.txt";
  const std::wstring suggested_ext = L"txt";

  const std::wstring actual_filename = AppendExtensionIfNeeded(filename,
      filter_selected, suggested_ext);

  ASSERT_EQ(L"sample.html", actual_filename);
}

TEST(AppendExtensionIfNeeded, OnlyPeriods_ExtensionAppended) {
  const std::wstring filename = L"...";
  const std::wstring filter_selected = L"*.txt";
  const std::wstring suggested_ext = L"txt";

  const std::wstring actual_filename = AppendExtensionIfNeeded(filename,
      filter_selected, suggested_ext);

  ASSERT_EQ(L"...txt", actual_filename);
}

TEST(AppendExtensionIfNeeded, EqualToExtension_ExtensionAppended) {
  const std::wstring filename = L"txt";
  const std::wstring filter_selected = L"*.txt";
  const std::wstring suggested_ext = L"txt";

  const std::wstring actual_filename = AppendExtensionIfNeeded(filename,
      filter_selected, suggested_ext);

  ASSERT_EQ(L"txt.txt", actual_filename);
}

TEST(AppendExtensionIfNeeded, AllFilesFilter_NoExtensionAppended) {
  const std::wstring filename = L"sample.unknown-mime-type";
  const std::wstring filter_selected = L"*.*";
  const std::wstring suggested_ext;

  const std::wstring actual_filename = AppendExtensionIfNeeded(filename,
      filter_selected, suggested_ext);

  ASSERT_EQ(L"sample.unknown-mime-type", actual_filename);
}
