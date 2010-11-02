// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utility class to manage user scripts.

#include "ceee/ie/plugin/scripting/userscripts_librarian.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"

UserScriptsLibrarian::UserScriptsLibrarian() {
}

UserScriptsLibrarian::~UserScriptsLibrarian() {
}

HRESULT UserScriptsLibrarian::AddUserScripts(
    const UserScriptList& user_scripts) {
  // Note that we read the userscript files from disk every time that a frame
  // loads (one BHO and one librarian per frame). This offers us the advantage
  // of not having to reload scripts into memory when an extension autoupdates
  // in Chrome. If we decide to cache these scripts in memory, then we will need
  // a Chrome extension autoupdate automation notification.

  size_t i = user_scripts_.size();
  user_scripts_.insert(user_scripts_.end(), user_scripts.begin(),
                       user_scripts.end());
  for (; i < user_scripts_.size(); ++i) {
    UserScript& user_script = user_scripts_[i];
    UserScript::FileList& js_scripts = user_script.js_scripts();
    LoadFiles(&js_scripts);
    UserScript::FileList& css_scripts = user_script.css_scripts();
    LoadFiles(&css_scripts);
  }

  return S_OK;
}

bool UserScriptsLibrarian::HasMatchingUserScripts(const GURL& url) const {
  for (size_t i = 0; i < user_scripts_.size(); ++i) {
    const UserScript& user_script = user_scripts_[i];
    if (user_script.MatchesUrl(url))
      return true;
  }
  return false;
}

HRESULT UserScriptsLibrarian::GetMatchingUserScriptsCssContent(
    const GURL& url, bool require_all_frames, std::string* css_content) const {
  DCHECK(css_content);
  if (!css_content)
    return E_POINTER;

  for (size_t i = 0; i < user_scripts_.size(); ++i) {
    const UserScript& user_script = user_scripts_[i];
    if (!user_script.MatchesUrl(url) ||
        (require_all_frames && !user_script.match_all_frames()))
      continue;

    for (size_t j = 0; j < user_script.css_scripts().size(); ++j) {
      const UserScript::File& file = user_script.css_scripts()[j];
      *css_content += file.GetContent().as_string();
    }
  }

  return S_OK;
}

HRESULT UserScriptsLibrarian::GetMatchingUserScriptsJsContent(
    const GURL& url, UserScript::RunLocation location, bool require_all_frames,
    JsFileList* js_file_list) {
  DCHECK(js_file_list);
  if (!js_file_list)
    return E_POINTER;

  if (!user_scripts_.empty()) {
    for (size_t i = 0; i < user_scripts_.size(); ++i) {
      const UserScript& user_script = user_scripts_[i];

      // TODO(ericdingle@chromium.org): Remove the fourth and fifth
      // conditions once DOCUMENT_IDLE is supported.
      if (!user_script.MatchesUrl(url) ||
          (require_all_frames && !user_script.match_all_frames()) ||
          user_script.run_location() != location &&
          (user_script.run_location() != UserScript::DOCUMENT_IDLE ||
           location != UserScript::DOCUMENT_END))
        continue;

      for (size_t j = 0; j < user_script.js_scripts().size(); ++j) {
        const UserScript::File& file = user_script.js_scripts()[j];

        js_file_list->push_back(JsFile());
        JsFile& js_file = (*js_file_list)[js_file_list->size()-1];
        js_file.file_path =
            file.extension_root().Append(file.relative_path()).value();
        js_file.content = file.GetContent().as_string();
      }
    }
  }

  return S_OK;
}

void UserScriptsLibrarian::LoadFiles(UserScript::FileList* file_list) {
  for (size_t i = 0; i < file_list->size(); ++i) {
    UserScript::File& script_file = (*file_list)[i];
    // The content may have been set manually (e.g., for unittests).
    // So we first check if they need to be loaded from the file, or not.
    if (script_file.GetContent().empty()) {
      std::string content;
      file_util::ReadFileToString(
          script_file.extension_root().Append(
              script_file.relative_path()), &content);
      script_file.set_content(content);
    }
  }
}
