// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SCRIPTS_RUN_INFO_H_
#define EXTENSIONS_RENDERER_SCRIPTS_RUN_INFO_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/macros.h"
#include "base/timer/elapsed_timer.h"
#include "extensions/common/user_script.h"

namespace blink {
class WebFrame;
}

namespace extensions {

// A struct containing information about a script run.
struct ScriptsRunInfo {
  // Map of extensions IDs to the executing script paths.
  typedef std::map<std::string, std::set<std::string> > ExecutingScriptsMap;

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

  // Log information about a given script run.
  void LogRun(blink::WebFrame* web_frame, UserScript::RunLocation location);

 private:
  DISALLOW_COPY_AND_ASSIGN(ScriptsRunInfo);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_SCRIPTS_RUN_INFO_H_
