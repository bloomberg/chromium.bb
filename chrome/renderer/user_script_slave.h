// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_USER_SCRIPT_SLAVE_H_
#define CHROME_RENDERER_USER_SCRIPT_SLAVE_H_
#pragma once

#include <set>
#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/stl_util-inl.h"
#include "base/string_piece.h"
#include "chrome/common/extensions/user_script.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptSource.h"

class ExtensionSet;

namespace WebKit {
class WebFrame;
}

using WebKit::WebScriptSource;

// Manages installed UserScripts for a render process.
class UserScriptSlave {
 public:
  UserScriptSlave(const ExtensionSet* extensions);
  ~UserScriptSlave();

  // Returns the unique set of extension IDs this UserScriptSlave knows about.
  void GetActiveExtensions(std::set<std::string>* extension_ids);

  // Update the parsed scripts from shared memory.
  bool UpdateScripts(base::SharedMemoryHandle shared_memory);

  // Inject the appropriate scripts into a frame based on its URL.
  // TODO(aa): Extract a UserScriptFrame interface out of this to improve
  // testability.
  void InjectScripts(WebKit::WebFrame* frame, UserScript::RunLocation location);

  static int GetIsolatedWorldId(const std::string& extension_id);

  static void InsertInitExtensionCode(std::vector<WebScriptSource>* sources,
                                      const std::string& extension_id);
 private:
  // Shared memory containing raw script data.
  scoped_ptr<base::SharedMemory> shared_memory_;

  // Parsed script data.
  std::vector<UserScript*> scripts_;
  STLElementDeleter<std::vector<UserScript*> > script_deleter_;

  // Greasemonkey API source that is injected with the scripts.
  base::StringPiece api_js_;

  // Extension metadata.
  const ExtensionSet* extensions_;

  DISALLOW_COPY_AND_ASSIGN(UserScriptSlave);
};

#endif  // CHROME_RENDERER_USER_SCRIPT_SLAVE_H_
