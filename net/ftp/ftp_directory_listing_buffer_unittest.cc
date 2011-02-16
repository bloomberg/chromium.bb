// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_directory_listing_buffer.h"

#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "net/base/net_errors.h"
#include "net/ftp/ftp_directory_listing_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(FtpDirectoryListingBufferTest, Parse) {
  const char* test_files[] = {
    "dir-listing-ls-1",
    "dir-listing-ls-1-utf8",
    "dir-listing-ls-2",
    "dir-listing-ls-3",
    "dir-listing-ls-4",
    "dir-listing-ls-5",
    "dir-listing-ls-6",
    "dir-listing-ls-7",
    "dir-listing-ls-8",
    "dir-listing-ls-9",
    "dir-listing-ls-10",
    "dir-listing-ls-11",
    "dir-listing-ls-12",
    "dir-listing-ls-13",
    "dir-listing-ls-14",
    "dir-listing-ls-15",
    "dir-listing-ls-16",
    "dir-listing-ls-17",
    "dir-listing-ls-18",
    "dir-listing-ls-19",
    "dir-listing-ls-20",  // TODO(phajdan.jr): should use windows-1251 encoding.
    "dir-listing-ls-21",  // TODO(phajdan.jr): should use windows-1251 encoding.
    "dir-listing-ls-22",  // TODO(phajdan.jr): should use windows-1251 encoding.
    "dir-listing-ls-23",
    "dir-listing-ls-24",
    "dir-listing-netware-1",
    "dir-listing-netware-2",
    "dir-listing-vms-1",
    "dir-listing-vms-2",
    "dir-listing-vms-3",
    "dir-listing-vms-4",
    "dir-listing-vms-5",
    "dir-listing-windows-1",
    "dir-listing-windows-2",
  };

  FilePath test_dir;
  PathService::Get(base::DIR_SOURCE_ROOT, &test_dir);
  test_dir = test_dir.AppendASCII("net");
  test_dir = test_dir.AppendASCII("data");
  test_dir = test_dir.AppendASCII("ftp");

  base::Time::Exploded mock_current_time_exploded = { 0 };
  mock_current_time_exploded.year = 1994;
  mock_current_time_exploded.month = 11;
  mock_current_time_exploded.day_of_month = 15;
  mock_current_time_exploded.hour = 12;
  mock_current_time_exploded.minute = 45;
  base::Time mock_current_time(
      base::Time::FromLocalExploded(mock_current_time_exploded));

  for (size_t i = 0; i < arraysize(test_files); i++) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s", i, test_files[i]));

    net::FtpDirectoryListingBuffer buffer(mock_current_time);

    std::string test_listing;
    EXPECT_TRUE(file_util::ReadFileToString(test_dir.AppendASCII(test_files[i]),
                                            &test_listing));

    EXPECT_EQ(net::OK, buffer.ConsumeData(test_listing.data(),
                                          test_listing.length()));
    EXPECT_EQ(net::OK, buffer.ProcessRemainingData());

    std::string expected_listing;
    ASSERT_TRUE(file_util::ReadFileToString(
        test_dir.AppendASCII(std::string(test_files[i]) + ".expected"),
        &expected_listing));

    std::vector<std::string> lines;
    StringTokenizer tokenizer(expected_listing, "\r\n");
    while (tokenizer.GetNext())
      lines.push_back(tokenizer.token());
    ASSERT_EQ(0U, lines.size() % 8);

    for (size_t i = 0; i < lines.size() / 8; i++) {
      std::string type(lines[8 * i]);
      std::string name(lines[8 * i + 1]);
      int64 size;
      base::StringToInt64(lines[8 * i + 2], &size);

      SCOPED_TRACE(base::StringPrintf("Filename: %s", name.c_str()));

      int year, month, day_of_month, hour, minute;
      base::StringToInt(lines[8 * i + 3], &year);
      base::StringToInt(lines[8 * i + 4], &month);
      base::StringToInt(lines[8 * i + 5], &day_of_month);
      base::StringToInt(lines[8 * i + 6], &hour);
      base::StringToInt(lines[8 * i + 7], &minute);

      ASSERT_TRUE(buffer.EntryAvailable());
      net::FtpDirectoryListingEntry entry = buffer.PopEntry();

      if (type == "d") {
        EXPECT_EQ(net::FtpDirectoryListingEntry::DIRECTORY, entry.type);
      } else if (type == "-") {
        EXPECT_EQ(net::FtpDirectoryListingEntry::FILE, entry.type);
      } else if (type == "l") {
        EXPECT_EQ(net::FtpDirectoryListingEntry::SYMLINK, entry.type);
      } else {
        ADD_FAILURE() << "invalid gold test data: " << type;
      }

      EXPECT_EQ(UTF8ToUTF16(name), entry.name);
      EXPECT_EQ(size, entry.size);

      base::Time::Exploded time_exploded;
      entry.last_modified.LocalExplode(&time_exploded);
      EXPECT_EQ(year, time_exploded.year);
      EXPECT_EQ(month, time_exploded.month);
      EXPECT_EQ(day_of_month, time_exploded.day_of_month);
      EXPECT_EQ(hour, time_exploded.hour);
      EXPECT_EQ(minute, time_exploded.minute);
    }
    EXPECT_FALSE(buffer.EntryAvailable());
  }
}

}  // namespace
