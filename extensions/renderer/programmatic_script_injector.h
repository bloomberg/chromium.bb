// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_PROGRAMMATIC_SCRIPT_INJECTOR_H_
#define EXTENSIONS_RENDERER_PROGRAMMATIC_SCRIPT_INJECTOR_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "extensions/renderer/script_injection.h"
#include "url/gurl.h"

struct ExtensionMsg_ExecuteCode_Params;

namespace blink {
class WebFrame;
}

namespace content {
class RenderView;
}

namespace extensions {

// A ScriptInjector to handle tabs.executeScript().
class ProgrammaticScriptInjector : public ScriptInjector {
 public:
  ProgrammaticScriptInjector(const ExtensionMsg_ExecuteCode_Params& params,
                             blink::WebFrame* web_frame);
  ~ProgrammaticScriptInjector() override;

 private:
  // ScriptInjector implementation.
  UserScript::InjectionType script_type() const override;
  bool ShouldExecuteInChildFrames() const override;
  bool ShouldExecuteInMainWorld() const override;
  bool IsUserGesture() const override;
  bool ExpectsResults() const override;
  bool ShouldInjectJs(UserScript::RunLocation run_location) const override;
  bool ShouldInjectCss(UserScript::RunLocation run_location) const override;
  PermissionsData::AccessType CanExecuteOnFrame(
      const InjectionHost* injection_host,
      blink::WebFrame* web_frame,
      int tab_id,
      const GURL& top_url) const override;
  std::vector<blink::WebScriptSource> GetJsSources(
      UserScript::RunLocation run_location) const override;
  std::vector<std::string> GetCssSources(
      UserScript::RunLocation run_location) const override;
  void GetRunInfo(ScriptsRunInfo* scripts_run_info,
                  UserScript::RunLocation run_location) const override;
  void OnInjectionComplete(scoped_ptr<base::ListValue> execution_results,
                           UserScript::RunLocation run_location) override;
  void OnWillNotInject(InjectFailureReason reason) override;

  // Return the run location for this injector.
  UserScript::RunLocation GetRunLocation() const;

  // Notify the browser that the script was injected (or never will be), and
  // send along any results or errors.
  void Finish(const std::string& error);

  // The parameters for injecting the script.
  scoped_ptr<ExtensionMsg_ExecuteCode_Params> params_;

  // The url of the frame into which we are injecting.
  GURL url_;

  // The RenderView to which we send the response upon completion.
  content::RenderView* render_view_;

  // The results of the script execution.
  scoped_ptr<base::ListValue> results_;

  // Whether or not this script injection has finished.
  bool finished_;

  DISALLOW_COPY_AND_ASSIGN(ProgrammaticScriptInjector);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_PROGRAMMATIC_SCRIPT_INJECTOR_H_
