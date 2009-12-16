// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/firefox_importer_utils.h"

#include <algorithm>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/search_engines/template_url_parser.h"
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
  DISALLOW_EVIL_CONSTRUCTORS(FirefoxURLParameterFilter);
};
}  // namespace

bool GetFirefoxVersionAndPathFromProfile(const std::wstring& profile_path,
                                         int* version,
                                         std::wstring* app_path) {
  bool ret = false;
  std::wstring compatibility_file(profile_path);
  file_util::AppendToPath(&compatibility_file, L"compatibility.ini");
  std::string content;
  file_util::ReadFileToString(compatibility_file, &content);
  ReplaceSubstringsAfterOffset(&content, 0, "\r\n", "\n");
  std::vector<std::string> lines;
  SplitString(content, '\n', &lines);

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
        *app_path = UTF8ToWide(line.substr(equal + 1));
      }
    }
  }
  return ret;
}

void ParseProfileINI(std::wstring file, DictionaryValue* root) {
  // Reads the whole INI file.
  std::string content;
  file_util::ReadFileToString(file, &content);
  ReplaceSubstringsAfterOffset(&content, 0, "\r\n", "\n");
  std::vector<std::string> lines;
  SplitString(content, '\n', &lines);

  // Parses the file.
  root->Clear();
  std::wstring current_section;
  for (size_t i = 0; i < lines.size(); ++i) {
    std::wstring line = UTF8ToWide(lines[i]);
    if (line.empty()) {
      // Skips the empty line.
      continue;
    }
    if (line[0] == L'#' || line[0] == L';') {
      // This line is a comment.
      continue;
    }
    if (line[0] == L'[') {
      // It is a section header.
      current_section = line.substr(1);
      size_t end = current_section.rfind(L']');
      if (end != std::wstring::npos)
        current_section.erase(end);
    } else {
      std::wstring key, value;
      size_t equal = line.find(L'=');
      if (equal != std::wstring::npos) {
        key = line.substr(0, equal);
        value = line.substr(equal + 1);
        // Checks whether the section and key contain a '.' character.
        // Those sections and keys break DictionaryValue's path format,
        // so we discard them.
        if (current_section.find(L'.') == std::wstring::npos &&
            key.find(L'.') == std::wstring::npos)
          root->SetString(current_section + L"." + key, value);
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

void ParseSearchEnginesFromXMLFiles(const std::vector<std::wstring>& xml_files,
                                    std::vector<TemplateURL*>* search_engines) {
  DCHECK(search_engines);

  std::map<std::wstring, TemplateURL*> search_engine_for_url;
  std::string content;
  // The first XML file represents the default search engine in Firefox 3, so we
  // need to keep it on top of the list.
  TemplateURL* default_turl = NULL;
  for (std::vector<std::wstring>::const_iterator file_iter = xml_files.begin();
       file_iter != xml_files.end(); ++file_iter) {
    file_util::ReadFileToString(*file_iter, &content);
    TemplateURL* template_url = new TemplateURL();
    FirefoxURLParameterFilter param_filter;
    if (TemplateURLParser::Parse(
        reinterpret_cast<const unsigned char*>(content.data()),
        content.length(), &param_filter, template_url) &&
        template_url->url()) {
      std::wstring url = template_url->url()->url();
      std::map<std::wstring, TemplateURL*>::iterator iter =
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
      template_url->set_keyword(
              TemplateURLModel::GenerateKeyword(GURL(WideToUTF8(url)), false));
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
  std::map<std::wstring, TemplateURL*>::iterator t_iter;
  for (t_iter = search_engine_for_url.begin();
       t_iter != search_engine_for_url.end(); ++t_iter) {
    if (t_iter->second == default_turl)
      search_engines->insert(search_engines->begin(), default_turl);
    else
      search_engines->push_back(t_iter->second);
  }
}

bool ReadPrefFile(const std::wstring& path_name,
                  const std::wstring& file_name,
                  std::string* content) {
  if (content == NULL)
    return false;

  std::wstring file = path_name;
  file_util::AppendToPath(&file, file_name.c_str());

  file_util::ReadFileToString(file, content);

  if (content->empty()) {
    NOTREACHED() << L"Firefox preference file " << file_name.c_str()
                 << L" is empty.";
    return false;
  }

  return true;
}

std::string ReadBrowserConfigProp(const std::wstring& app_path,
                                  const std::string& pref_key) {
  std::string content;
  if (!ReadPrefFile(app_path, L"browserconfig.properties", &content))
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
    NOTREACHED() << "Firefox property " << pref_key << " could not be parsed.";
    return "";
  }

  return content.substr(start + 1, stop - start - 1);
}

std::string ReadPrefsJsValue(const std::wstring& profile_path,
                             const std::string& pref_key) {
  std::string content;
  if (!ReadPrefFile(profile_path, L"prefs.js", &content))
    return "";

  // This file has the syntax: user_pref("key", value);
  std::string search_for = std::string("user_pref(\"") + pref_key +
                           std::string("\", ");
  size_t prop_index = content.find(search_for);
  if (prop_index == std::string::npos)
    return "";

  size_t start = prop_index + search_for.length();
  size_t stop = std::string::npos;
  if (start != std::string::npos)
    stop = content.find(")", start + 1);

  if (start == std::string::npos || stop == std::string::npos) {
    NOTREACHED() << "Firefox property " << pref_key << " could not be parsed.";
    return "";
  }

  // String values have double quotes we don't need to return to the caller.
  if (content[start] == '\"' && content[stop - 1] == '\"') {
    ++start;
    --stop;
  }

  return content.substr(start, stop - start);
}

int GetFirefoxDefaultSearchEngineIndex(
    const std::vector<TemplateURL*>& search_engines,
    const std::wstring& profile_path) {
  // The default search engine is contained in the file prefs.js found in the
  // profile directory.
  // It is the "browser.search.selectedEngine" property.
  if (search_engines.empty())
    return -1;

  std::wstring default_se_name = UTF8ToWide(
      ReadPrefsJsValue(profile_path, "browser.search.selectedEngine"));

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
    if (default_se_name == (*iter)->short_name()) {
      default_se_index = static_cast<int>(iter - search_engines.begin());
      break;
    }
  }
  if (default_se_index == -1) {
    NOTREACHED() <<
        "Firefox default search engine not found in search engine list";
  }

  return default_se_index;
}

GURL GetHomepage(const std::wstring& profile_path) {
  std::string home_page_list =
      ReadPrefsJsValue(profile_path, "browser.startup.homepage");

  size_t seperator = home_page_list.find_first_of('|');
  if (seperator == std::string::npos)
    return GURL(home_page_list);

  return GURL(home_page_list.substr(0, seperator));
}

bool IsDefaultHomepage(const GURL& homepage,
                       const std::wstring& app_path) {
  if (!homepage.is_valid())
    return false;

  std::string default_homepages =
      ReadBrowserConfigProp(app_path, "browser.startup.homepage");

  size_t seperator = default_homepages.find_first_of('|');
  if (seperator == std::string::npos)
    return homepage.spec() == GURL(default_homepages).spec();

  // Crack the string into separate homepage urls.
  std::vector<std::string> urls;
  SplitString(default_homepages, '|', &urls);

  for (size_t i = 0; i < urls.size(); ++i) {
    if (homepage.spec() == GURL(urls[i]).spec())
      return true;
  }

  return false;
}
