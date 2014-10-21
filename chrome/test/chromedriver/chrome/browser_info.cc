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

  std::string browser;
  if (!dict->GetString("Browser", &browser)) {
    return Status(kUnknownError,
                  "version info doesn't include string 'Browser'");
  }

  std::string blink_version;
  if (!dict->GetString("WebKit-Version", &blink_version)) {
    return Status(kUnknownError,
                  "version info doesn't include string 'WebKit-Version'");
  }

  Status status = ParseBrowserString(browser, &browser_info->browser_name,
      &browser_info->browser_version, &browser_info->build_no);

  if (status.IsError())
    return status;

  return ParseBlinkVersionString(blink_version, &browser_info->blink_revision);
}

Status ParseBrowserString(const std::string& browser_string,
                          std::string* browser_name,
                          std::string* browser_version,
                          int* build_no) {
  if (browser_string.empty()) {
    *browser_name = "content shell";
    return Status(kOk);
  }

  if (browser_string.find("Version/") == 0u) {
    *browser_name = "webview";
    return Status(kOk);
  }

  std::string prefix = "Chrome/";
  if (browser_string.find(prefix) == 0u) {
    *browser_name = "chrome";
    *browser_version = browser_string.substr(prefix.length());

    std::vector<std::string> version_parts;
    base::SplitString(*browser_version, '.', &version_parts);
    if (version_parts.size() != 4 ||
        !base::StringToInt(version_parts[2], build_no)) {
      return Status(kUnknownError,
                    "unrecognized Chrome version: " + *browser_version);
    }

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
