// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SCRIPT_INJECTION_H_
#define EXTENSIONS_RENDERER_SCRIPT_INJECTION_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/macros.h"
#include "base/timer/elapsed_timer.h"
#include "extensions/common/user_script.h"

namespace blink {
class WebFrame;
}

namespace extensions {
class Extension;

// An abstract script wrapper which is aware of whether or not it is allowed
// to execute, and contains the implementation to do so.
class ScriptInjection {
 public:
  // Map of extensions IDs to the executing script paths.
  typedef std::map<std::string, std::set<std::string> > ExecutingScriptsMap;

  // A struct containing information about a script run.
  struct ScriptsRunInfo {
    ScriptsRunInfo();
    ~ScriptsRunInfo();

    // The number of CSS scripts injected.
    size_t num_css;
    // The number of JS scripts injected.
    size_t num_js;
    // A map of extension ids to executing script paths.
    ExecutingScriptsMap executing_scripts;
    // The elapsed time since the ScriptsRunInfo was constructed.
    base::ElapsedTimer timer;

   private:
    DISALLOW_COPY_AND_ASSIGN(ScriptsRunInfo);
  };

  // Gets the isolated world ID to use for the given |extension| in the given
  // |frame|. If no isolated world has been created for that extension,
  // one will be created and initialized.
  static int GetIsolatedWorldIdForExtension(const Extension* extension,
                                            blink::WebFrame* web_frame);

  // Return the id of the extension associated with the given world.
  static std::string GetExtensionIdForIsolatedWorld(int world_id);

  // Remove the isolated world associated with the given extension.
  static void RemoveIsolatedWorld(const std::string& extension_id);

  ScriptInjection(blink::WebFrame* web_frame,
                  const std::string& extension_id,
                  UserScript::RunLocation run_location,
                  int tab_id);
  virtual ~ScriptInjection();

  // Try to inject the script at the |current_location|. This returns true if
  // the script has either injected or will never inject (i.e., if the object
  // is done), and false if injection is delayed (either for permission purposes
  // or because |current_location| is not the designated |run_location_|).
  // NOTE: |extension| may be NULL, if the extension is removed!
  bool TryToInject(UserScript::RunLocation current_location,
                   const Extension* extension,
                   ScriptsRunInfo* scripts_run_info);

  // Called when permission for the given injection has been granted.
  // Returns true if the injection ran.
  virtual bool OnPermissionGranted(const Extension* extension,
                                   ScriptsRunInfo* scripts_run_info);

  // Accessors.
  const blink::WebFrame* web_frame() const { return web_frame_; }
  blink::WebFrame* web_frame() { return web_frame_; }
  UserScript::RunLocation run_location() const { return run_location_; }
  const std::string& extension_id() const { return extension_id_; }
  int tab_id() const { return tab_id_; }
  int64 request_id() const { return request_id_; }
  void set_request_id(int64 request_id) { request_id_ = request_id; }

 protected:
  // The possible reasons for not injecting the script.
  enum InjectFailureReason {
    EXTENSION_REMOVED,  // The extension was removed before injection.
    NOT_ALLOWED,        // The script is not allowed to inject.
    WONT_INJECT         // The injection won't inject because the user rejected
                        // (or just did not accept) the injection.
  };

  // The possible types of access for a given frame.
  enum AccessType {
    DENY_ACCESS,    // The script cannot access the given frame.
    ALLOW_ACCESS,   // The script can access the given frame.
    REQUEST_ACCESS  // The browser must determine if the script can access the
                    // given frame.
  };

  // Returns whether or not the script is allowed to run.
  virtual AccessType Allowed(const Extension* extension) const = 0;

  // Injects the script, optionally populating |scripts_run_info|.
  virtual void Inject(const Extension* extension,
                      ScriptsRunInfo* scripts_run_info) = 0;

  // Handles the case that the script will never inject (e.g., notifying
  // listeners that the injection is "complete"). |reason| contains the reason
  // injection will not occur.
  virtual void OnWillNotInject(InjectFailureReason reason) {};

 private:
  // Send a message to the browser requesting permission to execute.
  void RequestPermission();

  // Notify that we will not inject, and mark it as acknowledged.
  void NotifyWillNotInject(InjectFailureReason reason);

  // Inject the script and mark this injection as |complete_|.
  void InjectAndMarkComplete(const Extension* extension,
                             ScriptsRunInfo* scripts_run_info);

  // The (main) WebFrame into which this should inject the script.
  blink::WebFrame* web_frame_;

  // The id of the associated extension.
  std::string extension_id_;

  // The location in the document load at which we inject the script.
  UserScript::RunLocation run_location_;

  // The tab id associated with the frame.
  int tab_id_;

  // This injection's request id. This will be -1 unless the injection is
  // currently waiting on permission.
  int64 request_id_;

  // Whether or not the injection is complete, either via injecting the script
  // or because it will never complete.
  bool complete_;

  DISALLOW_COPY_AND_ASSIGN(ScriptInjection);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_SCRIPT_INJECTION_H_
