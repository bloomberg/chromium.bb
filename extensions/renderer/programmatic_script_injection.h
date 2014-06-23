// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_PROGRAMMATIC_SCRIPT_INJECTION_H_
#define EXTENSIONS_RENDERER_PROGRAMMATIC_SCRIPT_INJECTION_H_

#include "base/macros.h"
#include "extensions/renderer/script_injection.h"

class GURL;
struct ExtensionMsg_ExecuteCode_Params;

namespace blink {
class WebFrame;
}

namespace content {
class RenderView;
}

namespace extensions {
class Extension;

// A ScriptInjection to handle tabs.executeScript().
class ProgrammaticScriptInjection : public ScriptInjection {
 public:
  ProgrammaticScriptInjection(blink::WebFrame* web_frame,
                              const ExtensionMsg_ExecuteCode_Params& params,
                              int tab_id);
  virtual ~ProgrammaticScriptInjection();

 private:
  // ScriptInjection implementation.
  virtual bool TryToInject(UserScript::RunLocation current_location,
                           const Extension* extension,
                           ScriptsRunInfo* scripts_run_info) OVERRIDE;
  virtual bool OnPermissionGranted(const Extension* extension,
                                   ScriptsRunInfo* scripts_run_info) OVERRIDE;

  // Returns the associated RenderView.
  content::RenderView* GetRenderView();

  // Returns true if the script can execute on the given |frame|.
  // |top_url| is passed in for efficiency when this is used in a loop.
  bool CanExecuteOnFrame(const Extension* extension,
                         const blink::WebFrame* frame,
                         const GURL& top_url) const;

  void Inject(const Extension* extension, ScriptsRunInfo* scripts_run_info);

  // The parameters for injecting the script.
  scoped_ptr<ExtensionMsg_ExecuteCode_Params> params_;

  DISALLOW_COPY_AND_ASSIGN(ProgrammaticScriptInjection);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_PROGRAMMATIC_SCRIPT_INJECTION_H_
