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
class Extension;

// A ScriptInjector to handle tabs.executeScript().
class ProgrammaticScriptInjector : public ScriptInjector {
 public:
  ProgrammaticScriptInjector(const ExtensionMsg_ExecuteCode_Params& params,
                             blink::WebFrame* web_frame);
  virtual ~ProgrammaticScriptInjector();

 private:
  // ScriptInjector implementation.
  virtual UserScript::InjectionType script_type() const OVERRIDE;
  virtual bool ShouldExecuteInChildFrames() const OVERRIDE;
  virtual bool ShouldExecuteInMainWorld() const OVERRIDE;
  virtual bool IsUserGesture() const OVERRIDE;
  virtual bool ExpectsResults() const OVERRIDE;
  virtual bool ShouldInjectJs(
      UserScript::RunLocation run_location) const OVERRIDE;
  virtual bool ShouldInjectCss(
      UserScript::RunLocation run_location) const OVERRIDE;
  virtual PermissionsData::AccessType CanExecuteOnFrame(
      const Extension* extension,
      blink::WebFrame* web_frame,
      int tab_id,
      const GURL& top_url) const OVERRIDE;
  virtual std::vector<blink::WebScriptSource> GetJsSources(
      UserScript::RunLocation run_location) const OVERRIDE;
  virtual std::vector<std::string> GetCssSources(
      UserScript::RunLocation run_location) const OVERRIDE;
  virtual void OnInjectionComplete(
      scoped_ptr<base::ListValue> execution_results,
      ScriptsRunInfo* scripts_run_info,
      UserScript::RunLocation run_location) OVERRIDE;
  virtual void OnWillNotInject(InjectFailureReason reason) OVERRIDE;

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
