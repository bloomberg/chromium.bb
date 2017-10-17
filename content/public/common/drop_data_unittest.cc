// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/drop_data.h"

#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "net/base/mime_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/strings/utf_string_conversions.h"
#define CONVERT_IF_NEEDED(x) base::UTF8ToUTF16((x))
#else
#define CONVERT_IF_NEEDED(x) x
#endif

namespace content {

TEST(DropDataTest, GetSafeFilenameForImageFileContents) {
  static constexpr struct {
    const char* const extension;
    const bool is_well_known;
    const bool should_generate_filename;
  } kTestCases[] = {
      // Extensions with a well-known but non-image MIME type should not result
      // in the generation of a filename.
      {"exe", true, false},
      {"html", true, false},
      {"js", true, false},
      // Extensions that do not have a well-known MIME type should not result in
      // the generation of a filename.
      {"should-not-be-known-extension", false, false},
      // Extensions with a well-known MIME type should result in the generation
      // of a filename.
      {"bmp", true, true},
      {"gif", true, true},
      {"ico", true, true},
      {"jpg", true, true},
      {"png", true, true},
      {"svg", true, true},
      {"tif", true, true},
      {"xbm", true, true},
      {"webp", true, true},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.extension);
    std::string ignored;
    ASSERT_EQ(test_case.is_well_known,
              net::GetWellKnownMimeTypeFromExtension(
                  CONVERT_IF_NEEDED(test_case.extension), &ignored));

    DropData drop_data;
    drop_data.file_contents_source_url =
        GURL(base::StringPrintf("https://example.com/testresource"));
    drop_data.file_contents_filename_extension =
        CONVERT_IF_NEEDED(test_case.extension);
    base::Optional<base::FilePath> generated_name =
        drop_data.GetSafeFilenameForImageFileContents();
    ASSERT_EQ(test_case.should_generate_filename, generated_name.has_value());

    if (test_case.should_generate_filename) {
      // base::FilePath::Extension() returns the preceeding dot, so drop it
      // before doing the comparison.
      EXPECT_EQ(CONVERT_IF_NEEDED(test_case.extension),
                generated_name->Extension().substr(1));
    }
  }
}

}  // namespace content

#undef CONVERT_IF_NEEDED
