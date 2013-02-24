// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/firefox_importer_utils.h"

#include <algorithm>
#include <map>
#include <string>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_parser.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// FirefoxURLParameterFilter is used to remove parameter mentioning Firefox from
// the search URL when importing search engines.
class FirefoxURLParameterFilter : public TemplateURLParser::ParameterFilter {
 public:
  FirefoxURLParameterFilter() {}
  virtual ~FirefoxURLParameterFilter() {}

  // TemplateURLParser::ParameterFilter method.
  virtual bool KeepParameter(const std::string& key,
                             const std::string& value) OVERRIDE {
    std::string low_value = StringToLowerASCII(value);
    if (low_value.find("mozilla") != std::string::npos ||
        low_value.find("firefox") != std::string::npos ||
        low_value.find("moz:") != std::string::npos )
      return false;
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FirefoxURLParameterFilter);
};
}  // namespace

base::FilePath GetFirefoxProfilePath() {
  DictionaryValue root;
  base::FilePath ini_file = GetProfilesINI();
  ParseProfileINI(ini_file, &root);

  base::FilePath source_path;
  for (int i = 0; ; ++i) {
    std::string current_profile = StringPrintf("Profile%d", i);
    if (!root.HasKey(current_profile)) {
      // Profiles are continuously numbered. So we exit when we can't
      // find the i-th one.
      break;
    }
    std::string is_relative;
    string16 path16;
    if (root.GetStringASCII(current_profile + ".IsRelative", &is_relative) &&
        root.GetString(current_profile + ".Path", &path16)) {
#if defined(OS_WIN)
      ReplaceSubstringsAfterOffset(
          &path16, 0, ASCIIToUTF16("/"), ASCIIToUTF16("\\"));
#endif
      base::FilePath path =
          base::FilePath::FromWStringHack(UTF16ToWide(path16));

      // IsRelative=1 means the folder path would be relative to the
      // path of profiles.ini. IsRelative=0 refers to a custom profile
      // location.
      if (is_relative == "1") {
        path = ini_file.DirName().Append(path);
      }

      // We only import the default profile when multiple profiles exist,
      // since the other profiles are used mostly by developers for testing.
      // Otherwise, Profile0 will be imported.
      std::string is_default;
      if ((root.GetStringASCII(current_profile + ".Default", &is_default) &&
           is_default == "1") || i == 0) {
        // We have found the default profile.
        return path;
      }
    }
  }
  return base::FilePath();
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

void ParseProfileINI(const base::FilePath& file, DictionaryValue* root) {
  // Reads the whole INI file.
  std::string content;
  file_util::ReadFileToString(file, &content);
  ReplaceSubstringsAfterOffset(&content, 0, "\r\n", "\n");
  std::vector<std::string> lines;
  base::SplitString(content, '\n', &lines);

  // Parses the file.
  root->Clear();
  std::string current_section;
  for (size_t i = 0; i < lines.size(); ++i) {
    std::string line = lines[i];
    if (line.empty()) {
      // Skips the empty line.
      continue;
    }
    if (line[0] == '#' || line[0] == ';') {
      // This line is a comment.
      continue;
    }
    if (line[0] == '[') {
      // It is a section header.
      current_section = line.substr(1);
      size_t end = current_section.rfind(']');
      if (end != std::string::npos)
        current_section.erase(end);
    } else {
      std::string key, value;
      size_t equal = line.find('=');
      if (equal != std::string::npos) {
        key = line.substr(0, equal);
        value = line.substr(equal + 1);
        // Checks whether the section and key contain a '.' character.
        // Those sections and keys break DictionaryValue's path format,
        // so we discard them.
        if (current_section.find('.') == std::string::npos &&
            key.find('.') == std::string::npos)
          root->SetString(current_section + "." + key, value);
      }
    }
  }
}

bool CanImportURL(const GURL& url) {
  const char* kInvalidSchemes[] = {"wyciwyg", "place", "about", "chrome"};

  // The URL is not valid.
  if (!url.is_valid())
    return false;

  // Filter out the URLs with unsupported schemes.
  for (size_t i = 0; i < arraysize(kInvalidSchemes); ++i) {
    if (url.SchemeIs(kInvalidSchemes[i]))
      return false;
  }

  return true;
}

void ParseSearchEnginesFromXMLFiles(
    const std::vector<base::FilePath>& xml_files,
    std::vector<TemplateURL*>* search_engines) {
  DCHECK(search_engines);

  typedef std::map<std::string, TemplateURL*> SearchEnginesMap;
  SearchEnginesMap search_engine_for_url;
  std::string content;
  // The first XML file represents the default search engine in Firefox 3, so we
  // need to keep it on top of the list.
  SearchEnginesMap::const_iterator default_turl = search_engine_for_url.end();
  for (std::vector<base::FilePath>::const_iterator file_iter =
           xml_files.begin(); file_iter != xml_files.end(); ++file_iter) {
    file_util::ReadFileToString(*file_iter, &content);
    FirefoxURLParameterFilter param_filter;
    TemplateURL* template_url = TemplateURLParser::Parse(NULL, true,
        content.data(), content.length(), &param_filter);
    if (template_url) {
      SearchEnginesMap::iterator iter =
          search_engine_for_url.find(template_url->url());
      if (iter == search_engine_for_url.end()) {
        iter = search_engine_for_url.insert(
            std::make_pair(template_url->url(), template_url)).first;
      } else {
        // We have already found a search engine with the same URL.  We give
        // priority to the latest one found, as GetSearchEnginesXMLFiles()
        // returns a vector with first Firefox default search engines and then
        // the user's ones.  We want to give priority to the user ones.
        delete iter->second;
        iter->second = template_url;
      }
      if (default_turl == search_engine_for_url.end())
        default_turl = iter;
    }
    content.clear();
  }

  // Put the results in the |search_engines| vector.
  for (SearchEnginesMap::iterator t_iter = search_engine_for_url.begin();
       t_iter != search_engine_for_url.end(); ++t_iter) {
    if (t_iter == default_turl)
      search_engines->insert(search_engines->begin(), default_turl->second);
    else
      search_engines->push_back(t_iter->second);
  }
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
    return "";

  // This file has the syntax: key=value.
  size_t prop_index = content.find(pref_key + "=");
  if (prop_index == std::string::npos)
    return "";

  size_t start = prop_index + pref_key.length();
  size_t stop = std::string::npos;
  if (start != std::string::npos)
    stop = content.find("\n", start + 1);

  if (start == std::string::npos ||
      stop == std::string::npos || (start == stop)) {
    LOG(WARNING) << "Firefox property " << pref_key << " could not be parsed.";
    return "";
  }

  return content.substr(start + 1, stop - start - 1);
}

std::string ReadPrefsJsValue(const base::FilePath& profile_path,
                             const std::string& pref_key) {
  std::string content;
  if (!ReadPrefFile(profile_path.AppendASCII("prefs.js"), &content))
    return "";

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

bool ParsePrefFile(const base::FilePath& pref_file, DictionaryValue* prefs) {
  // The string that is before a pref key.
  const std::string kUserPrefString = "user_pref(\"";
  std::string contents;
  if (!file_util::ReadFileToString(pref_file, &contents))
    return false;

  std::vector<std::string> lines;
  Tokenize(contents, "\n", &lines);

  for (std::vector<std::string>::const_iterator iter = lines.begin();
       iter != lines.end(); ++iter) {
    const std::string& line = *iter;
    size_t start_key = line.find(kUserPrefString);
    if (start_key == std::string::npos)
      continue;  // Could be a comment or a blank line.
    start_key += kUserPrefString.length();
    size_t stop_key = line.find('"', start_key);
    if (stop_key == std::string::npos) {
      LOG(ERROR) << "Invalid key found in Firefox pref file '" <<
          pref_file.value() << "' line is '" << line << "'.";
      continue;
    }
    std::string key = line.substr(start_key, stop_key - start_key);
    size_t start_value = line.find(',', stop_key + 1);
    if (start_value == std::string::npos) {
      LOG(ERROR) << "Invalid value found in Firefox pref file '" <<
          pref_file.value() << "' line is '" << line << "'.";
      continue;
    }
    size_t stop_value = line.find(");", start_value + 1);
    if (stop_value == std::string::npos) {
      LOG(ERROR) << "Invalid value found in Firefox pref file '" <<
          pref_file.value() << "' line is '" << line << "'.";
      continue;
    }
    std::string value = line.substr(start_value + 1,
                                    stop_value - start_value - 1);
    TrimWhitespace(value, TRIM_ALL, &value);
    // Value could be a boolean.
    bool is_value_true = LowerCaseEqualsASCII(value, "true");
    if (is_value_true || LowerCaseEqualsASCII(value, "false")) {
      prefs->SetBoolean(key, is_value_true);
      continue;
    }

    // Value could be a string.
    if (value.size() >= 2U &&
        value[0] == '"' && value[value.size() - 1] == '"') {
      value = value.substr(1, value.size() - 2);
      // ValueString only accept valid UTF-8.  Simply ignore that entry if it is
      // not UTF-8.
      if (IsStringUTF8(value))
        prefs->SetString(key, value);
      else
        VLOG(1) << "Non UTF8 value for key " << key << ", ignored.";
      continue;
    }

    // Or value could be an integer.
    int int_value = 0;
    if (base::StringToInt(value, &int_value)) {
      prefs->SetInteger(key, int_value);
      continue;
    }

    LOG(ERROR) << "Invalid value found in Firefox pref file '"
               << pref_file.value() << "' value is '" << value << "'.";
  }
  return true;
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
    return "";
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
  if (file_util::PathExists(app_ini_file)) {
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
