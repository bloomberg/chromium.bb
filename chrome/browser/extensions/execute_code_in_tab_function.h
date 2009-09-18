// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXECUTE_CODE_IN_TAB_FUNCTION_H__
#define CHROME_BROWSER_EXTENSIONS_EXECUTE_CODE_IN_TAB_FUNCTION_H__

#include <string>
#include <vector>

#include "base/file_path.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_registrar.h"

class MessageLoop;

// Implement API call tabs.executeScript and tabs.insertCSS.
class ExecuteCodeInTabFunction : public AsyncExtensionFunction,
                                 public NotificationObserver {
 public:
  ExecuteCodeInTabFunction() : execute_tab_id_(-1), ui_loop_(NULL) {}

 private:
  virtual bool RunImpl();

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Load contents from file whose path is specified in JSON arguments. Run
  // in file thread.
  void LoadFile();

  // Run in UI thread.
  void Execute();

  NotificationRegistrar registrar_;

  // Id of tab which executes code.
  int execute_tab_id_;

  MessageLoop* ui_loop_;

  // Contain path of file which is specified in JSON arguments.
  FilePath file_path_;

  // Contain code to be executed.
  std::string code_string_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXECUTE_CODE_IN_TAB_FUNCTION_H__
