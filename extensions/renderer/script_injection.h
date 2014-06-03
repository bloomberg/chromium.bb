// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SCRIPT_INJECTION_H_
#define EXTENSIONS_RENDERER_SCRIPT_INJECTION_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/timer/elapsed_timer.h"
#include "extensions/common/user_script.h"

class GURL;

namespace blink {
class WebFrame;
}

namespace content {
class RenderView;
}

namespace extensions {
class UserScriptSlave;

// This class is a wrapper around a UserScript that knows how to inject itself
// into a frame.
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

  // Return the URL to use as the document url when checking permissions for
  // script injection.
  static GURL GetDocumentUrlForFrame(blink::WebFrame* frame);

  ScriptInjection(scoped_ptr<UserScript> script,
                  UserScriptSlave* user_script_slave);
  ~ScriptInjection();

  // Inject the script into the given |frame| if the script should run on the
  // frame and has permission to do so. If the script requires user consent,
  // this will register a pending request to inject at a later time.
  // If the script is run immediately, |scripts_run_info| is updated with
  // information about the run.
  void InjectIfAllowed(blink::WebFrame* frame,
                       UserScript::RunLocation location,
                       const GURL& document_url,
                       ScriptsRunInfo* scripts_run_info);

  // If a request with the given |request_id| exists, runs that request and
  // modifies |scripts_run_info| with information about the run. Otherwise, does
  // nothing.
  // If |frame_out| is non-NULL and a script was run, |frame_out| will be
  // populated with the frame in which the script was run.
  // Returns true if the request was found *and* the script was run.
  bool NotifyScriptPermitted(int64 request_id,
                             content::RenderView* render_view,
                             ScriptsRunInfo* scripts_run_info,
                             blink::WebFrame** frame_out);

  // Notififies the Injection that the frame has been detached (i.e. is about
  // to be destroyed).
  void FrameDetached(blink::WebFrame* frame);

  void SetScript(scoped_ptr<UserScript> script);

  const std::string& extension_id() { return extension_id_; }
  const UserScript* script() { return script_.get(); }

 private:
  struct PendingInjection;

  // Returns true if this ScriptInjection wants to run on the given |frame| at
  // the given |run_location| (i.e., if this script would inject either JS or
  // CSS).
  bool WantsToRun(blink::WebFrame* frame,
                  UserScript::RunLocation run_location,
                  const GURL& document_url) const;

  // Returns true if the script will inject [css|js] at the given
  // |run_location|.
  bool ShouldInjectJS(UserScript::RunLocation run_location) const;
  bool ShouldInjectCSS(UserScript::RunLocation run_location) const;

  // Injects the script into the given |frame|, and updates |scripts_run_info|
  // information about the run.
  void Inject(blink::WebFrame* frame,
              UserScript::RunLocation run_location,
              ScriptsRunInfo* scripts_run_info) const;

  // Injects the [css|js] scripts into the frame, and stores the results of
  // the run in |scripts_run_info|.
  void InjectJS(blink::WebFrame* frame, ScriptsRunInfo* scripts_run_info) const;
  void InjectCSS(blink::WebFrame* frame, ScriptsRunInfo* scripts_run_info)
      const;

  // The UserScript this is injecting.
  scoped_ptr<UserScript> script_;

  // The associated extension's id.
  std::string extension_id_;

  // The associated UserScriptSlave.
  // It's unfortunate that this is needed, but we use it to get the isolated
  // world ids and the associated extensions.
  // TODO(rdevlin.cronin): It would be nice to clean this up more.
  UserScriptSlave* user_script_slave_;

  // True if the script is a standalone script or emulates greasemonkey.
  bool is_standalone_or_emulate_greasemonkey_;

  ScopedVector<PendingInjection> pending_injections_;

  DISALLOW_COPY_AND_ASSIGN(ScriptInjection);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_SCRIPT_INJECTION_H_
