// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SCRIPT_EXECUTOR_IMPL_H_
#define CHROME_BROWSER_EXTENSIONS_SCRIPT_EXECUTOR_IMPL_H_
#pragma once

#include "chrome/browser/extensions/script_executor.h"

#include "base/compiler_specific.h"

namespace extensions {

// Standard implementation of ScriptExecutor which just sends an IPC to
// |web_contents_| to execute the script.
class ScriptExecutorImpl : public ScriptExecutor {
 public:
  explicit ScriptExecutorImpl(content::WebContents* web_contents);

  virtual ~ScriptExecutorImpl();

  virtual void ExecuteScript(const std::string& extension_id,
                             ScriptType script_type,
                             const std::string& code,
                             FrameScope frame_scope,
                             UserScript::RunLocation run_at,
                             WorldType world_type,
                             const ExecuteScriptCallback& callback) OVERRIDE;
 private:
  // The next value to use for request_id inExtensionMsg_ExecuteCode_Params.
  int next_request_id_;

  // The WebContents this is bound to.
  content::WebContents* web_contents_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SCRIPT_EXECUTOR_IMPL_H_
