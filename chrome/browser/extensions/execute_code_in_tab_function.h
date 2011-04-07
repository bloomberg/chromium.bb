// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXECUTE_CODE_IN_TAB_FUNCTION_H__
#define CHROME_BROWSER_EXTENSIONS_EXECUTE_CODE_IN_TAB_FUNCTION_H__
#pragma once

#include <string>

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/common/extensions/extension_resource.h"
#include "content/browser/tab_contents/tab_contents_observer.h"

// Implement API call tabs.executeScript and tabs.insertCSS.
class ExecuteCodeInTabFunction : public AsyncExtensionFunction,
                                 public TabContentsObserver {
 public:
  ExecuteCodeInTabFunction();
  virtual ~ExecuteCodeInTabFunction();

 private:
  virtual bool RunImpl();

  // TabContentsObserver overrides.
  virtual bool OnMessageReceived(const IPC::Message& message);

  // Message handler.
  void OnExecuteCodeFinished(int request_id, bool success,
                             const std::string& error);

  // Called when contents from the file whose path is specified in JSON
  // arguments has been loaded.
  void DidLoadFile(bool success, const std::string& data);

  // Run in UI thread.  Code string contains the code to be executed. Returns
  // true on success. If true is returned, this does an AddRef.
  bool Execute(const std::string& code_string);

  TabContentsObserver::Registrar registrar_;

  // Id of tab which executes code.
  int execute_tab_id_;

  // Contains extension resource built from path of file which is
  // specified in JSON arguments.
  ExtensionResource resource_;

  // If all_frames_ is true, script or CSS text would be injected
  // to all frames; Otherwise only injected to top main frame.
  bool all_frames_;
};

class TabsExecuteScriptFunction : public ExecuteCodeInTabFunction {
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.executeScript")
};

class TabsInsertCSSFunction : public ExecuteCodeInTabFunction {
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.insertCSS")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXECUTE_CODE_IN_TAB_FUNCTION_H__
