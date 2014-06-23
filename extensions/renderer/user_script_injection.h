// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_USER_SCRIPT_INJECTION_H_
#define EXTENSIONS_RENDERER_USER_SCRIPT_INJECTION_H_

#include "extensions/renderer/script_injection.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/scoped_observer.h"
#include "extensions/common/user_script.h"
#include "extensions/renderer/user_script_set.h"

namespace blink {
class WebFrame;
}

namespace extensions {
class Extension;

// A ScriptInjection for UserScripts.
class UserScriptInjection : public ScriptInjection,
                            public UserScriptSet::Observer {
 public:
  UserScriptInjection(blink::WebFrame* web_frame,
                      const std::string& extension_id,
                      UserScript::RunLocation run_location,
                      int tab_id,
                      UserScriptSet* script_list,
                      const UserScript* script);
  virtual ~UserScriptInjection();

 private:
  // ScriptInjection implementation.
  virtual bool TryToInject(UserScript::RunLocation current_location,
                           const Extension* extension,
                           ScriptsRunInfo* scripts_run_info) OVERRIDE;
  virtual bool OnPermissionGranted(const Extension* extension,
                                   ScriptsRunInfo* scripts_run_info) OVERRIDE;

  // UserScriptSet::Observer implementation.
  virtual void OnUserScriptsUpdated(
      const std::set<std::string>& changed_extensions,
      const std::vector<UserScript*>& scripts) OVERRIDE;

  // Returns true if the injection is allowed to run.
  bool Allowed(const Extension* extension);

  // Requests permission for the injection. Returns true if this should inject
  // immediately.
  bool RequestPermission(const Extension* extension);

  // Injects the script.
  void Inject(const Extension* extension, ScriptsRunInfo* scripts_run_info);

  // Inject the JS/CSS specified in |script_|.
  void InjectJS(const Extension* extension, ScriptsRunInfo* scripts_run_info);
  void InjectCSS(ScriptsRunInfo* scripts_run_info);

  // The associated user script. Owned by the UserScriptInjection that created
  // this object.
  const UserScript* script_;

  // The id of the associated user script. We cache this because when we update
  // the |script_| associated with this injection, the old referance may be
  // deleted.
  int64 script_id_;

  ScopedObserver<UserScriptSet, UserScriptSet::Observer>
      user_script_set_observer_;

  DISALLOW_COPY_AND_ASSIGN(UserScriptInjection);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_USER_SCRIPT_INJECTION_H_
