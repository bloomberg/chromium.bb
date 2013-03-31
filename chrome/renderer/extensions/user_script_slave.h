// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_USER_SCRIPT_SLAVE_H_
#define CHROME_RENDERER_EXTENSIONS_USER_SCRIPT_SLAVE_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "chrome/common/extensions/user_script.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptSource.h"

class ExtensionSet;
class GURL;

namespace WebKit {
class WebFrame;
}

using WebKit::WebScriptSource;

namespace extensions {
class Extension;

// Manages installed UserScripts for a render process.
class UserScriptSlave {
 public:
  // Utility to get the URL we will match against for a frame. If the frame has
  // committed, this is the commited URL. Otherwise it is the provisional URL.
  static GURL GetDataSourceURLForFrame(const WebKit::WebFrame* frame);

  explicit UserScriptSlave(const ExtensionSet* extensions);
  ~UserScriptSlave();

  // Returns the unique set of extension IDs this UserScriptSlave knows about.
  void GetActiveExtensions(std::set<std::string>* extension_ids);

  // Update the parsed scripts from shared memory.
  bool UpdateScripts(base::SharedMemoryHandle shared_memory);

  // Inject the appropriate scripts into a frame based on its URL.
  // TODO(aa): Extract a UserScriptFrame interface out of this to improve
  // testability.
  void InjectScripts(WebKit::WebFrame* frame, UserScript::RunLocation location);

  // Gets the isolated world ID to use for the given |extension| in the given
  // |frame|. If no isolated world has been created for that extension,
  // one will be created and initialized.
  int GetIsolatedWorldIdForExtension(const Extension* extension,
                                     WebKit::WebFrame* frame);

  // Gets the id of the extension running in a given isolated world. If no such
  // isolated world exists, or no extension is running in it, returns empty
  // string.
  std::string GetExtensionIdForIsolatedWorld(int isolated_world_id);

  void RemoveIsolatedWorld(const std::string& extension_id);

 private:
  static void InitializeIsolatedWorld(int isolated_world_id,
                                      const Extension* extension);

  // Shared memory containing raw script data.
  scoped_ptr<base::SharedMemory> shared_memory_;

  // Parsed script data.
  std::vector<UserScript*> scripts_;
  STLElementDeleter<std::vector<UserScript*> > script_deleter_;

  // Greasemonkey API source that is injected with the scripts.
  base::StringPiece api_js_;

  // Extension metadata.
  const ExtensionSet* extensions_;

  typedef std::map<std::string, int> IsolatedWorldMap;
  IsolatedWorldMap isolated_world_ids_;

  DISALLOW_COPY_AND_ASSIGN(UserScriptSlave);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_USER_SCRIPT_SLAVE_H_
