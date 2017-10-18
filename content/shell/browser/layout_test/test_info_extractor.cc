// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/test_info_extractor.h"

#include <iostream>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "net/base/filename_util.h"

namespace content {

namespace {

#if defined(OS_ANDROID)
// On Android, all passed tests will be paths to a local temporary directory.
// However, because we can't transfer all test files to the device, translate
// those paths to a local, forwarded URL so the host can serve them.
bool GetTestUrlForAndroid(std::string& path_or_url, GURL* url) {
  // Path to search for when translating a layout test path to an URL.
  const char kAndroidLayoutTestPath[] =
      "/data/local/tmp/third_party/WebKit/LayoutTests/";
  // The base URL from which layout tests are being served on Android.
  const char kAndroidLayoutTestBase[] = "http://127.0.0.1:8000/all-tests/";

  if (path_or_url.find(kAndroidLayoutTestPath) == std::string::npos)
    return false;

  std::string test_location(kAndroidLayoutTestBase);
  test_location.append(path_or_url.substr(strlen(kAndroidLayoutTestPath)));

  *url = GURL(test_location);
  return true;
}
#endif  // defined(OS_ANDROID)

std::unique_ptr<TestInfo> GetTestInfoFromLayoutTestName(
    const std::string& test_name) {
  // A test name is formated like file:///path/to/test'--pixel-test'pixelhash
  std::string path_or_url = test_name;
  std::string pixel_switch;
  std::string::size_type separator_position = path_or_url.find('\'');
  if (separator_position != std::string::npos) {
    pixel_switch = path_or_url.substr(separator_position + 1);
    path_or_url.erase(separator_position);
  }
  separator_position = pixel_switch.find('\'');
  std::string expected_pixel_hash;
  if (separator_position != std::string::npos) {
    expected_pixel_hash = pixel_switch.substr(separator_position + 1);
    pixel_switch.erase(separator_position);
  }
  const bool enable_pixel_dumping =
      (pixel_switch == "--pixel-test" || pixel_switch == "-p");

  GURL test_url;
#if defined(OS_ANDROID)
  if (GetTestUrlForAndroid(path_or_url, &test_url)) {
    return base::MakeUnique<TestInfo>(test_url, enable_pixel_dumping,
                                      expected_pixel_hash, base::FilePath());
  }
#endif

  test_url = GURL(path_or_url);
  if (!(test_url.is_valid() && test_url.has_scheme())) {
    // We're outside of the message loop here, and this is a test.
    base::ScopedAllowBlockingForTesting allow_blocking;
#if defined(OS_WIN)
    base::FilePath::StringType wide_path_or_url =
        base::SysNativeMBToWide(path_or_url);
    base::FilePath local_file(wide_path_or_url);
#else
    base::FilePath local_file(path_or_url);
#endif
    if (!base::PathExists(local_file)) {
      base::FilePath base_path;
      PathService::Get(base::DIR_SOURCE_ROOT, &base_path);
      local_file = base_path.Append(FILE_PATH_LITERAL("third_party"))
                       .Append(FILE_PATH_LITERAL("WebKit"))
                       .Append(FILE_PATH_LITERAL("LayoutTests"))
                       .Append(local_file);
    }
    test_url = net::FilePathToFileURL(base::MakeAbsoluteFilePath(local_file));
  }
  base::FilePath local_path;
  base::FilePath current_working_directory;

  // We're outside of the message loop here, and this is a test.
  base::ScopedAllowBlockingForTesting allow_blocking;
  if (net::FileURLToFilePath(test_url, &local_path))
    current_working_directory = local_path.DirName();
  else
    base::GetCurrentDirectory(&current_working_directory);
  return base::MakeUnique<TestInfo>(test_url, enable_pixel_dumping,
                                    expected_pixel_hash,
                                    current_working_directory);
}

}  // namespace

TestInfo::TestInfo(const GURL& url,
                   bool enable_pixel_dumping,
                   const std::string& expected_pixel_hash,
                   const base::FilePath& current_working_directory)
    : url(url),
      enable_pixel_dumping(enable_pixel_dumping),
      expected_pixel_hash(expected_pixel_hash),
      current_working_directory(current_working_directory) {}
TestInfo::~TestInfo() {}

TestInfoExtractor::TestInfoExtractor(
    const base::CommandLine::StringVector& cmd_args)
    : cmdline_args_(cmd_args), cmdline_position_(0) {}

TestInfoExtractor::~TestInfoExtractor() {}

std::unique_ptr<TestInfo> TestInfoExtractor::GetNextTest() {
  if (cmdline_position_ >= cmdline_args_.size())
    return nullptr;

  std::string test_string;
  if (cmdline_args_[cmdline_position_] == FILE_PATH_LITERAL("-")) {
    do {
      bool success = !!std::getline(std::cin, test_string, '\n');
      if (!success)
        return nullptr;
    } while (test_string.empty());
  } else {
#if defined(OS_WIN)
    test_string = base::WideToUTF8(cmdline_args_[cmdline_position_++]);
#else
    test_string = cmdline_args_[cmdline_position_++];
#endif
  }

  DCHECK(!test_string.empty());
  if (test_string == "QUIT")
    return nullptr;
  return GetTestInfoFromLayoutTestName(test_string);
}

}  // namespace content
