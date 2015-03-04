// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SCRIPT_INJECTION_H_
#define EXTENSIONS_RENDERER_SCRIPT_INJECTION_H_

#include "base/basictypes.h"
#include "base/macros.h"
#include "extensions/common/user_script.h"
#include "extensions/renderer/injection_host.h"
#include "extensions/renderer/script_injector.h"

struct HostID;

namespace blink {
class WebLocalFrame;
}

namespace extensions {
struct ScriptsRunInfo;

// A script wrapper which is aware of whether or not it is allowed to execute,
// and contains the implementation to do so.
class ScriptInjection {
 public:
  // Return the id of the injection host associated with the given world.
  static std::string GetHostIdForIsolatedWorld(int world_id);

  // Remove the isolated world associated with the given injection host.
  static void RemoveIsolatedWorld(const std::string& host_id);

  ScriptInjection(scoped_ptr<ScriptInjector> injector,
                  blink::WebLocalFrame* web_frame,
                  scoped_ptr<const InjectionHost> injection_host,
                  UserScript::RunLocation run_location,
                  int tab_id);
  ~ScriptInjection();

  // Try to inject the script at the |current_location|. This returns true if
  // the script has either injected or will never inject (i.e., if the object
  // is done), and false if injection is delayed (either for permission purposes
  // or because |current_location| is not the designated |run_location_|).
  bool TryToInject(UserScript::RunLocation current_location,
                   ScriptsRunInfo* scripts_run_info);

  // Called when permission for the given injection has been granted.
  // Returns true if the injection ran.
  bool OnPermissionGranted(ScriptsRunInfo* scripts_run_info);

  // Resets the pointer of the injection host when the host is gone.
  void OnHostRemoved();

  // Accessors.
  blink::WebLocalFrame* web_frame() const { return web_frame_; }
  const HostID& host_id() const { return injection_host_->id(); }
  int64 request_id() const { return request_id_; }

 private:
  // Sends a message to the browser, either that the script injection would
  // like to inject, or to notify the browser that it is currently injecting.
  void SendInjectionMessage(bool request_permission);

  // Injects the script, optionally populating |scripts_run_info|.
  void Inject(ScriptsRunInfo* scripts_run_info);

  // Inject any JS scripts into the |frame|, optionally populating
  // |execution_results|.
  void InjectJs(blink::WebLocalFrame* frame,
                base::ListValue* execution_results);

  // Inject any CSS source into the |frame|.
  void InjectCss(blink::WebLocalFrame* frame);

  // Notify that we will not inject, and mark it as acknowledged.
  void NotifyWillNotInject(ScriptInjector::InjectFailureReason reason);

  // The injector for this injection.
  scoped_ptr<ScriptInjector> injector_;

  // The (main) WebFrame into which this should inject the script.
  blink::WebLocalFrame* web_frame_;

  // The associated injection host.
  scoped_ptr<const InjectionHost> injection_host_;

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
