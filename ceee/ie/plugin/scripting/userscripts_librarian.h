// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utility class to maintain user scripts and their matching URLs.

#ifndef CEEE_IE_PLUGIN_SCRIPTING_USERSCRIPTS_LIBRARIAN_H_
#define CEEE_IE_PLUGIN_SCRIPTING_USERSCRIPTS_LIBRARIAN_H_

#include <windows.h>

#include <string>
#include <vector>

#include "chrome/common/extensions/user_script.h"
#include "googleurl/src/gurl.h"


// The manager of UserScripts responsible for loading them from an extension
// and then returning the ones that match a given URL.
// TODO(mad@chromium.org): Find a way to reuse code from
// chrome/renderer/user_script_slave.
class UserScriptsLibrarian {
 public:
  typedef struct {
    std::wstring file_path;
    std::string content;
  } JsFile;
  typedef std::vector<JsFile> JsFileList;

  UserScriptsLibrarian();
  ~UserScriptsLibrarian();

  // Adds a list of users scripts to our current list.
  HRESULT AddUserScripts(const UserScriptList& user_scripts);

  // Identifies if we have any userscript that would match the given URL.
  bool HasMatchingUserScripts(const GURL& url) const;

  // Retrieve the CSS content from user scripts that match the given URL.
  // @param url The URL to match.
  // @param require_all_frames Whether to require the all_frames property of the
  //        user script to be true.
  // @param css_content The single stream of CSS content.
  HRESULT GetMatchingUserScriptsCssContent(
      const GURL& url,
      bool require_all_frames,
      std::string* css_content) const;

  // Retrieve the JS content from user scripts that match the given URL.
  // @param url The URL to match.
  // @param location The location where the scripts will be run at.
  // @param require_all_frames Whether to require the all_frames property of the
  //        user script to be true.
  // @param js_file_list A map of file names to JavaScript content to allow the
  //        caller to apply them individually (e.g., each with their own script
  //        engine).
  HRESULT GetMatchingUserScriptsJsContent(
      const GURL& url,
      UserScript::RunLocation location,
      bool require_all_frames,
      JsFileList* js_file_list);

 private:
  // A helper function to load the content of a script file if it has not
  // already been done, or explicitly set.
  void LoadFiles(UserScript::FileList* file_list);

  UserScriptList user_scripts_;
  DISALLOW_COPY_AND_ASSIGN(UserScriptsLibrarian);
};

#endif  // CEEE_IE_PLUGIN_SCRIPTING_USERSCRIPTS_LIBRARIAN_H_
