// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/search_engines/template_url_parser.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "googleurl/src/gurl.h"

namespace {

// FirefoxURLParameterFilter is used to remove parameter mentioning Firefox from
// the search URL when importing search engines.
class FirefoxURLParameterFilter : public TemplateURLParser::ParameterFilter {
 public:
  FirefoxURLParameterFilter() {}
  virtual ~FirefoxURLParameterFilter() {}

  // TemplateURLParser::ParameterFilter method.
  virtual bool KeepParameter(const std::string& key,
                             const std::string& value) {
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

FilePath GetFirefoxProfilePath() {
  DictionaryValue root;
  FilePath ini_file = GetProfilesINI();
  ParseProfileINI(ini_file, &root);

  FilePath source_path;
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
      FilePath path = FilePath::FromWStringHack(UTF16ToWide(path16));

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
  return FilePath();
}


bool GetFirefoxVersionAndPathFromProfile(const FilePath& profile_path,
                                         int* version,
                                         FilePath* app_path) {
  bool ret = false;
  FilePath compatibility_file = profile_path.AppendASCII("compatibility.ini");
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
        *version = line.substr(equal + 1)[0] - '0';
        ret = true;
      } else if (key == "LastAppDir") {
        // TODO(evanm): If the path in question isn't convertible to
        // UTF-8, what does Firefox do?  If it puts raw bytes in the
        // file, we could go straight from bytes -> filepath;
        // otherwise, we're out of luck here.
        *app_path = FilePath::FromWStringHack(
            UTF8ToWide(line.substr(equal + 1)));
      }
    }
  }
  return ret;
}

void ParseProfileINI(const FilePath& file, DictionaryValue* root) {
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

void ParseSearchEnginesFromXMLFiles(const std::vector<FilePath>& xml_files,
                                    std::vector<TemplateURL*>* search_engines) {
  DCHECK(search_engines);

  std::map<std::string, TemplateURL*> search_engine_for_url;
  std::string content;
  // The first XML file represents the default search engine in Firefox 3, so we
  // need to keep it on top of the list.
  TemplateURL* default_turl = NULL;
  for (std::vector<FilePath>::const_iterator file_iter = xml_files.begin();
       file_iter != xml_files.end(); ++file_iter) {
    file_util::ReadFileToString(*file_iter, &content);
    TemplateURL* template_url = new TemplateURL();
    FirefoxURLParameterFilter param_filter;
    if (TemplateURLParser::Parse(
        reinterpret_cast<const unsigned char*>(content.data()),
        content.length(), &param_filter, template_url) &&
        template_url->url()) {
      std::string url = template_url->url()->url();
      std::map<std::string, TemplateURL*>::iterator iter =
          search_engine_for_url.find(url);
      if (iter != search_engine_for_url.end()) {
        // We have already found a search engine with the same URL.  We give
        // priority to the latest one found, as GetSearchEnginesXMLFiles()
        // returns a vector with first Firefox default search engines and then
        // the user's ones.  We want to give priority to the user ones.
        delete iter->second;
        search_engine_for_url.erase(iter);
      }
      // Give this a keyword to facilitate tab-to-search, if possible.
      GURL gurl = GURL(url);
      template_url->set_keyword(
          TemplateURLModel::GenerateKeyword(gurl, false));
      template_url->set_logo_id(
          TemplateURLPrepopulateData::GetSearchEngineLogo(gurl));
      template_url->set_show_in_default_list(true);
      search_engine_for_url[url] = template_url;
      if (!default_turl)
        default_turl = template_url;
    } else {
      delete template_url;
    }
    content.clear();
  }

  // Put the results in the |search_engines| vector.
  std::map<std::string, TemplateURL*>::iterator t_iter;
  for (t_iter = search_engine_for_url.begin();
       t_iter != search_engine_for_url.end(); ++t_iter) {
    if (t_iter->second == default_turl)
      search_engines->insert(search_engines->begin(), default_turl);
    else
      search_engines->push_back(t_iter->second);
  }
}

bool ReadPrefFile(const FilePath& path, std::string* content) {
  if (content == NULL)
    return false;

  file_util::ReadFileToString(path, content);

  if (content->empty()) {
    LOG(WARNING) << "Firefox preference file " << path.value() << " is empty.";
    return false;
  }

  return true;
}

std::string ReadBrowserConfigProp(const FilePath& app_path,
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

std::string ReadPrefsJsValue(const FilePath& profile_path,
                             const std::string& pref_key) {
  std::string content;
  if (!ReadPrefFile(profile_path.AppendASCII("prefs.js"), &content))
    return "";

  return GetPrefsJsValue(content, pref_key);
}

int GetFirefoxDefaultSearchEngineIndex(
    const std::vector<TemplateURL*>& search_engines,
    const FilePath& profile_path) {
  // The default search engine is contained in the file prefs.js found in the
  // profile directory.
  // It is the "browser.search.selectedEngine" property.
  if (search_engines.empty())
    return -1;

  std::string default_se_name =
      ReadPrefsJsValue(profile_path, "browser.search.selectedEngine");

  if (default_se_name.empty()) {
    // browser.search.selectedEngine does not exist if the user has not changed
    // from the default (or has selected the default).
    // TODO: should fallback to 'browser.search.defaultengine' if selectedEngine
    // is empty.
    return -1;
  }

  int default_se_index = -1;
  for (std::vector<TemplateURL*>::const_iterator iter = search_engines.begin();
       iter != search_engines.end(); ++iter) {
    if (default_se_name == UTF16ToUTF8((*iter)->short_name())) {
      default_se_index = static_cast<int>(iter - search_engines.begin());
      break;
    }
  }
  if (default_se_index == -1) {
    LOG(WARNING) <<
        "Firefox default search engine not found in search engine list";
  }

  return default_se_index;
}

GURL GetHomepage(const FilePath& profile_path) {
  std::string home_page_list =
      ReadPrefsJsValue(profile_path, "browser.startup.homepage");

  size_t seperator = home_page_list.find_first_of('|');
  if (seperator == std::string::npos)
    return GURL(home_page_list);

  return GURL(home_page_list.substr(0, seperator));
}

bool IsDefaultHomepage(const GURL& homepage, const FilePath& app_path) {
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

bool ParsePrefFile(const FilePath& pref_file, DictionaryValue* prefs) {
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
