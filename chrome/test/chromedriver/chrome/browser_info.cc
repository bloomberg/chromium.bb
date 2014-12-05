// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/browser_info.h"

#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"

BrowserInfo::BrowserInfo()
    : browser_name(std::string()),
      browser_version(std::string()),
      build_no(kToTBuildNo),
      blink_revision(kToTBlinkRevision) {
}

BrowserInfo::BrowserInfo(std::string browser_name,
                         std::string browser_version,
                         int build_no,
                         int blink_revision)
    : browser_name(browser_name),
      browser_version(browser_version),
      build_no(build_no),
      blink_revision(blink_revision) {
}

Status ParseBrowserInfo(const std::string& data, BrowserInfo* browser_info) {
  scoped_ptr<base::Value> value(base::JSONReader::Read(data));
  if (!value.get())
    return Status(kUnknownError, "version info not in JSON");

  base::DictionaryValue* dict;
  if (!value->GetAsDictionary(&dict))
    return Status(kUnknownError, "version info not a dictionary");

  bool is_android = dict->HasKey("Android-Package");
  std::string browser_string;
  if (!dict->GetString("Browser", &browser_string))
    return Status(kUnknownError, "version doesn't include 'Browser'");

  Status status = ParseBrowserString(is_android, browser_string, browser_info);
  if (status.IsError())
    return status;

  std::string blink_version;
  if (!dict->GetString("WebKit-Version", &blink_version))
    return Status(kUnknownError, "version doesn't include 'WebKit-Version'");

  return ParseBlinkVersionString(blink_version, &browser_info->blink_revision);
}

Status ParseBrowserString(bool is_android,
                          const std::string& browser_string,
                          BrowserInfo* browser_info) {
  if (browser_string.empty()) {
    browser_info->browser_name = "content shell";
    return Status(kOk);
  }

  std::string prefix = "Chrome/";
  int build_no = 0;
  if (browser_string.find(prefix) == 0u) {
    std::string version = browser_string.substr(prefix.length());
    std::vector<std::string> version_parts;
    base::SplitString(version, '.', &version_parts);

    if (version_parts.size() != 4 ||
        !base::StringToInt(version_parts[2], &build_no)) {
      return Status(kUnknownError, "unrecognized Chrome version: " + version);
    }

    if (build_no != 0) {
      browser_info->browser_name = "chrome";
      browser_info->browser_version = browser_string.substr(prefix.length());
      browser_info->build_no = build_no;
      return Status(kOk);
    }
  }

  if (browser_string.find("Version/") == 0u ||  // KitKat
      (is_android && build_no == 0)) {          // Lollipop
    browser_info->browser_name = "webview";
    return Status(kOk);
  }

  return Status(kUnknownError,
                "unrecognized Chrome version: " + browser_string);
}

Status ParseBlinkVersionString(const std::string& blink_version,
                               int* blink_revision) {
  size_t before = blink_version.find('@');
  size_t after = blink_version.find(')');
  if (before == std::string::npos || after == std::string::npos) {
    return Status(kUnknownError,
                  "unrecognized Blink version string: " + blink_version);
  }

  // Chrome OS reports its Blink revision as a git hash. In this case, ignore it
  // and don't set |blink_revision|. For Chrome (and for Chrome OS) we use the
  // build number instead of the blink revision for decisions about backwards
  // compatibility.
  std::string revision = blink_version.substr(before + 1, after - before - 1);
  if (!IsGitHash(revision) && !base::StringToInt(revision, blink_revision)) {
    return Status(kUnknownError, "unrecognized Blink revision: " + revision);
  }

  return Status(kOk);
}

bool IsGitHash(const std::string& revision) {
  const int kShortGitHashLength = 7;
  const int kFullGitHashLength = 40;
  return kShortGitHashLength <= revision.size()
      && revision.size() <= kFullGitHashLength
      && base::ContainsOnlyChars(revision, "0123456789abcdefABCDEF");
}
