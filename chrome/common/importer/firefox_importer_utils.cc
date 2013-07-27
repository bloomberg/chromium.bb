// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/importer/firefox_importer_utils.h"

#include <algorithm>
#include <map>
#include <string>

#include "base/file_util.h"
#include "base/ini_parser.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

// Retrieves the file system path of the profile name.
base::FilePath GetProfilePath(const DictionaryValue& root,
                              const std::string& profile_name) {
  string16 path16;
  std::string is_relative;
  if (!root.GetStringASCII(profile_name + ".IsRelative", &is_relative) ||
      !root.GetString(profile_name + ".Path", &path16))
    return base::FilePath();

#if defined(OS_WIN)
  ReplaceSubstringsAfterOffset(
      &path16, 0, ASCIIToUTF16("/"), ASCIIToUTF16("\\"));
#endif
  base::FilePath path = base::FilePath::FromWStringHack(UTF16ToWide(path16));

  // IsRelative=1 means the folder path would be relative to the
  // path of profiles.ini. IsRelative=0 refers to a custom profile
  // location.
  if (is_relative == "1")
    path = GetProfilesINI().DirName().Append(path);

  return path;
}

// Checks if the named profile is the default profile.
bool IsDefaultProfile(const DictionaryValue& root,
                      const std::string& profile_name) {
  std::string is_default;
  root.GetStringASCII(profile_name + ".Default", &is_default);
  return is_default == "1";
}

} // namespace

base::FilePath GetFirefoxProfilePath() {
  base::FilePath ini_file = GetProfilesINI();
  std::string content;
  file_util::ReadFileToString(ini_file, &content);
  base::DictionaryValueINIParser ini_parser;
  ini_parser.Parse(content);
  return GetFirefoxProfilePathFromDictionary(ini_parser.root());
}

base::FilePath GetFirefoxProfilePathFromDictionary(
    const DictionaryValue& root) {
  std::vector<std::string> profiles;
  for (int i = 0; ; ++i) {
    std::string current_profile = base::StringPrintf("Profile%d", i);
    if (root.HasKey(current_profile)) {
      profiles.push_back(current_profile);
    } else {
      // Profiles are continuously numbered. So we exit when we can't
      // find the i-th one.
      break;
    }
  }

  if (profiles.empty())
    return base::FilePath();

  // When multiple profiles exist, the path to the default profile is returned,
  // since the other profiles are used mostly by developers for testing.
  for (std::vector<std::string>::const_iterator it = profiles.begin();
       it != profiles.end(); ++it)
    if (IsDefaultProfile(root, *it))
      return GetProfilePath(root, *it);

  // If no default profile is found, the path to Profile0 will be returned.
  return GetProfilePath(root, profiles.front());
}

bool GetFirefoxVersionAndPathFromProfile(const base::FilePath& profile_path,
                                         int* version,
                                         base::FilePath* app_path) {
  bool ret = false;
  base::FilePath compatibility_file =
      profile_path.AppendASCII("compatibility.ini");
  std::string content;
  file_util::ReadFileToString(compatibility_file, &content);
  ReplaceSubstringsAfterOffset(&content, 0, "\r\n", "\n");
  std::vector<std::string> lines;
  base::SplitString(content, '\n', &lines);

  for (size_t i = 0; i < lines.size(); ++i) {
    const std::string& line = lines[i];
    if (line.empty() || line[0] == '#' || line[0] == ';')
      continue;
    size_t equal = line.find('=');
    if (equal != std::string::npos) {
      std::string key = line.substr(0, equal);
      if (key == "LastVersion") {
        base::StringToInt(line.substr(equal + 1), version);
        ret = true;
      } else if (key == "LastAppDir") {
        // TODO(evanm): If the path in question isn't convertible to
        // UTF-8, what does Firefox do?  If it puts raw bytes in the
        // file, we could go straight from bytes -> filepath;
        // otherwise, we're out of luck here.
        *app_path = base::FilePath::FromWStringHack(
            UTF8ToWide(line.substr(equal + 1)));
      }
    }
  }
  return ret;
}

bool ReadPrefFile(const base::FilePath& path, std::string* content) {
  if (content == NULL)
    return false;

  file_util::ReadFileToString(path, content);

  if (content->empty()) {
    LOG(WARNING) << "Firefox preference file " << path.value() << " is empty.";
    return false;
  }

  return true;
}

std::string ReadBrowserConfigProp(const base::FilePath& app_path,
                                  const std::string& pref_key) {
  std::string content;
  if (!ReadPrefFile(app_path.AppendASCII("browserconfig.properties"), &content))
    return std::string();

  // This file has the syntax: key=value.
  size_t prop_index = content.find(pref_key + "=");
  if (prop_index == std::string::npos)
    return std::string();

  size_t start = prop_index + pref_key.length();
  size_t stop = std::string::npos;
  if (start != std::string::npos)
    stop = content.find("\n", start + 1);

  if (start == std::string::npos ||
      stop == std::string::npos || (start == stop)) {
    LOG(WARNING) << "Firefox property " << pref_key << " could not be parsed.";
    return std::string();
  }

  return content.substr(start + 1, stop - start - 1);
}

std::string ReadPrefsJsValue(const base::FilePath& profile_path,
                             const std::string& pref_key) {
  std::string content;
  if (!ReadPrefFile(profile_path.AppendASCII("prefs.js"), &content))
    return std::string();

  return GetPrefsJsValue(content, pref_key);
}

GURL GetHomepage(const base::FilePath& profile_path) {
  std::string home_page_list =
      ReadPrefsJsValue(profile_path, "browser.startup.homepage");

  size_t seperator = home_page_list.find_first_of('|');
  if (seperator == std::string::npos)
    return GURL(home_page_list);

  return GURL(home_page_list.substr(0, seperator));
}

bool IsDefaultHomepage(const GURL& homepage, const base::FilePath& app_path) {
  if (!homepage.is_valid())
    return false;

  std::string default_homepages =
      ReadBrowserConfigProp(app_path, "browser.startup.homepage");

  size_t seperator = default_homepages.find_first_of('|');
  if (seperator == std::string::npos)
    return homepage.spec() == GURL(default_homepages).spec();

  // Crack the string into separate homepage urls.
  std::vector<std::string> urls;
  base::SplitString(default_homepages, '|', &urls);

  for (size_t i = 0; i < urls.size(); ++i) {
    if (homepage.spec() == GURL(urls[i]).spec())
      return true;
  }

  return false;
}

std::string GetPrefsJsValue(const std::string& content,
                            const std::string& pref_key) {
  // This file has the syntax: user_pref("key", value);
  std::string search_for = std::string("user_pref(\"") + pref_key +
                           std::string("\", ");
  size_t prop_index = content.find(search_for);
  if (prop_index == std::string::npos)
    return std::string();

  size_t start = prop_index + search_for.length();
  size_t stop = std::string::npos;
  if (start != std::string::npos) {
    // Stop at the last ')' on this line.
    stop = content.find("\n", start + 1);
    stop = content.rfind(")", stop);
  }

  if (start == std::string::npos || stop == std::string::npos ||
      stop < start) {
    LOG(WARNING) << "Firefox property " << pref_key << " could not be parsed.";
    return std::string();
  }

  // String values have double quotes we don't need to return to the caller.
  if (content[start] == '\"' && content[stop - 1] == '\"') {
    ++start;
    --stop;
  }

  return content.substr(start, stop - start);
}

// The branding name is obtained from the application.ini file from the Firefox
// application directory. A sample application.ini file is the following:
//   [App]
//   Vendor=Mozilla
//   Name=Iceweasel
//   Profile=mozilla/firefox
//   Version=3.5.16
//   BuildID=20120421070307
//   Copyright=Copyright (c) 1998 - 2010 mozilla.org
//   ID={ec8030f7-c20a-464f-9b0e-13a3a9e97384}
//   .........................................
// In this example the function returns "Iceweasel" (or a localized equivalent).
string16 GetFirefoxImporterName(const base::FilePath& app_path) {
  const base::FilePath app_ini_file = app_path.AppendASCII("application.ini");
  std::string branding_name;
  if (base::PathExists(app_ini_file)) {
    std::string content;
    file_util::ReadFileToString(app_ini_file, &content);
    std::vector<std::string> lines;
    base::SplitString(content, '\n', &lines);
    const std::string name_attr("Name=");
    bool in_app_section = false;
    for (size_t i = 0; i < lines.size(); ++i) {
      TrimWhitespace(lines[i], TRIM_ALL, &lines[i]);
      if (lines[i] == "[App]") {
        in_app_section = true;
      } else if (in_app_section) {
        if (lines[i].find(name_attr) == 0) {
          branding_name = lines[i].substr(name_attr.size());
          break;
        } else if (lines[i].length() > 0 && lines[i][0] == '[') {
          // No longer in the [App] section.
          break;
        }
      }
    }
  }

  StringToLowerASCII(&branding_name);
  if (branding_name.find("iceweasel") != std::string::npos)
    return l10n_util::GetStringUTF16(IDS_IMPORT_FROM_ICEWEASEL);
  return l10n_util::GetStringUTF16(IDS_IMPORT_FROM_FIREFOX);
}
