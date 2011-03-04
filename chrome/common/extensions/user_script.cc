// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/user_script.h"

#include "base/pickle.h"
#include "base/string_util.h"

namespace {

bool UrlMatchesPatterns(const UserScript::PatternList* patterns,
                        const GURL& url) {
  for (UserScript::PatternList::const_iterator pattern = patterns->begin();
       pattern != patterns->end(); ++pattern) {
    if (pattern->MatchesUrl(url))
      return true;
  }

  return false;
}

bool UrlMatchesGlobs(const std::vector<std::string>* globs,
                     const GURL& url) {
  for (std::vector<std::string>::const_iterator glob = globs->begin();
       glob != globs->end(); ++glob) {
    if (MatchPattern(url.spec(), *glob))
      return true;
  }

  return false;
}

}  // namespace

// static
const char UserScript::kFileExtension[] = ".user.js";

// static
const int UserScript::kValidUserScriptSchemes =
    URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS |
    URLPattern::SCHEME_FILE | URLPattern::SCHEME_FTP;

bool UserScript::HasUserScriptFileExtension(const GURL& url) {
  return EndsWith(url.ExtractFileName(), kFileExtension, false);
}

bool UserScript::HasUserScriptFileExtension(const FilePath& path) {
  static FilePath extension(FilePath().AppendASCII(kFileExtension));
  return EndsWith(path.BaseName().value(), extension.value(), false);
}


UserScript::File::File(const FilePath& extension_root,
                       const FilePath& relative_path,
                       const GURL& url)
    : extension_root_(extension_root),
      relative_path_(relative_path),
      url_(url) {
}

UserScript::File::File() {}

UserScript::File::~File() {}

UserScript::UserScript()
    : run_location_(DOCUMENT_IDLE), emulate_greasemonkey_(false),
      match_all_frames_(false), incognito_enabled_(false),
      allow_file_access_(false) {
}

UserScript::~UserScript() {
}

void UserScript::add_url_pattern(const URLPattern& pattern) {
  url_patterns_.push_back(pattern);
}

void UserScript::clear_url_patterns() { url_patterns_.clear(); }

bool UserScript::MatchesUrl(const GURL& url) const {
  if (!url_patterns_.empty()) {
    if (!UrlMatchesPatterns(&url_patterns_, url))
      return false;
  }

  if (!globs_.empty()) {
    if (!UrlMatchesGlobs(&globs_, url))
      return false;
  }

  if (!exclude_globs_.empty()) {
    if (UrlMatchesGlobs(&exclude_globs_, url))
      return false;
  }

  return true;
}

void UserScript::File::Pickle(::Pickle* pickle) const {
  pickle->WriteString(url_.spec());
  // Do not write path. It's not needed in the renderer.
  // Do not write content. It will be serialized by other means.
}

void UserScript::File::Unpickle(const ::Pickle& pickle, void** iter) {
  // Read url.
  std::string url;
  CHECK(pickle.ReadString(iter, &url));
  set_url(GURL(url));
}

void UserScript::Pickle(::Pickle* pickle) const {
  // Write simple types.
  pickle->WriteInt(run_location());
  pickle->WriteString(extension_id());
  pickle->WriteBool(emulate_greasemonkey());
  pickle->WriteBool(match_all_frames());
  pickle->WriteBool(is_incognito_enabled());
  pickle->WriteBool(allow_file_access());

  // Write globs.
  std::vector<std::string>::const_iterator glob;
  pickle->WriteSize(globs_.size());
  for (glob = globs_.begin(); glob != globs_.end(); ++glob) {
    pickle->WriteString(*glob);
  }
  pickle->WriteSize(exclude_globs_.size());
  for (glob = exclude_globs_.begin(); glob != exclude_globs_.end(); ++glob) {
    pickle->WriteString(*glob);
  }

  // Write url patterns.
  pickle->WriteSize(url_patterns_.size());
  for (PatternList::const_iterator pattern = url_patterns_.begin();
       pattern != url_patterns_.end(); ++pattern) {
    pickle->WriteInt(pattern->valid_schemes());
    pickle->WriteString(pattern->GetAsString());
  }

  // Write js scripts.
  pickle->WriteSize(js_scripts_.size());
  for (FileList::const_iterator file = js_scripts_.begin();
    file != js_scripts_.end(); ++file) {
    file->Pickle(pickle);
  }

  // Write css scripts.
  pickle->WriteSize(css_scripts_.size());
  for (FileList::const_iterator file = css_scripts_.begin();
    file != css_scripts_.end(); ++file) {
    file->Pickle(pickle);
  }
}

void UserScript::Unpickle(const ::Pickle& pickle, void** iter) {
  // Read the run location.
  int run_location = 0;
  CHECK(pickle.ReadInt(iter, &run_location));
  CHECK(run_location >= 0 && run_location < RUN_LOCATION_LAST);
  run_location_ = static_cast<RunLocation>(run_location);

  CHECK(pickle.ReadString(iter, &extension_id_));
  CHECK(pickle.ReadBool(iter, &emulate_greasemonkey_));
  CHECK(pickle.ReadBool(iter, &match_all_frames_));
  CHECK(pickle.ReadBool(iter, &incognito_enabled_));
  CHECK(pickle.ReadBool(iter, &allow_file_access_));

  // Read globs.
  size_t num_globs = 0;
  CHECK(pickle.ReadSize(iter, &num_globs));
  globs_.clear();
  for (size_t i = 0; i < num_globs; ++i) {
    std::string glob;
    CHECK(pickle.ReadString(iter, &glob));
    globs_.push_back(glob);
  }

  CHECK(pickle.ReadSize(iter, &num_globs));
  exclude_globs_.clear();
  for (size_t i = 0; i < num_globs; ++i) {
    std::string glob;
    CHECK(pickle.ReadString(iter, &glob));
    exclude_globs_.push_back(glob);
  }

  // Read url patterns.
  size_t num_patterns = 0;
  CHECK(pickle.ReadSize(iter, &num_patterns));

  url_patterns_.clear();
  for (size_t i = 0; i < num_patterns; ++i) {
    int valid_schemes;
    CHECK(pickle.ReadInt(iter, &valid_schemes));
    std::string pattern_str;
    URLPattern pattern(valid_schemes);
    CHECK(pickle.ReadString(iter, &pattern_str));
    CHECK(URLPattern::PARSE_SUCCESS ==
          pattern.Parse(pattern_str, URLPattern::PARSE_LENIENT));
    url_patterns_.push_back(pattern);
  }

  // Read js scripts.
  size_t num_js_files = 0;
  CHECK(pickle.ReadSize(iter, &num_js_files));
  js_scripts_.clear();
  for (size_t i = 0; i < num_js_files; ++i) {
    File file;
    file.Unpickle(pickle, iter);
    js_scripts_.push_back(file);
  }

  // Read css scripts.
  size_t num_css_files = 0;
  CHECK(pickle.ReadSize(iter, &num_css_files));
  css_scripts_.clear();
  for (size_t i = 0; i < num_css_files; ++i) {
    File file;
    file.Unpickle(pickle, iter);
    css_scripts_.push_back(file);
  }
}
