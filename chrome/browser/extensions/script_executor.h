// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SCRIPT_EXECUTOR_H_
#define CHROME_BROWSER_EXTENSIONS_SCRIPT_EXECUTOR_H_
#pragma once

#include <string>

#include "base/callback_forward.h"
#include "chrome/common/extensions/user_script.h"

namespace content {
class WebContents;
}

namespace extensions {

// Interface for executing extension content scripts (e.g. executeScript) as
// described by the ExtensionMsg_ExecuteCode_Params IPC, and notifying the
// caller when responded with ExtensionHostMsg_ExecuteCodeFinished.
class ScriptExecutor {
 public:
  virtual ~ScriptExecutor() {}

  // The type of script being injected.
  enum ScriptType {
    JAVASCRIPT,
    CSS,
  };

  // The scope of the script injection across the frames.
  enum FrameScope {
    TOP_FRAME,
    ALL_FRAMES,
  };

  // The type of world to inject into (main world, or its own isolated world).
  enum WorldType {
    MAIN_WORLD,
    ISOLATED_WORLD,
  };

  // Callback from ExecuteScript. The arguments are (success, error).
  typedef base::Callback<void(bool, const std::string&)> ExecuteScriptCallback;

  // Executes a script. The arguments match ExtensionMsg_ExecuteCode_Params in
  // extension_messages.h (request_id is populated automatically).
  //
  // |callback| will always be called even if the IPC'd renderer is destroyed
  // before a response is received (in this case the callback will be with a
  // failure and appropriate error message).
  virtual void ExecuteScript(const std::string& extension_id,
                             ScriptType script_type,
                             const std::string& code,
                             FrameScope frame_scope,
                             UserScript::RunLocation run_at,
                             WorldType world_type,
                             const ExecuteScriptCallback& callback) = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SCRIPT_EXECUTOR_H_
