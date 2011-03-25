// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEVTOOLS_TAB_HELPER_H_
#define CHROME_BROWSER_DEBUGGER_DEVTOOLS_TAB_HELPER_H_
#pragma once

#include "content/browser/tab_contents/tab_contents_observer.h"

class DevToolsTabHelper : public TabContentsObserver {
 public:
  explicit DevToolsTabHelper(TabContents* tab_contents);
  virtual ~DevToolsTabHelper();

  // TabContentsObserver overrides.
  virtual bool OnMessageReceived(const IPC::Message& message);

 private:
  void OnForwardToAgent(const IPC::Message& message);
  void OnForwardToClient(const IPC::Message& message);
  void OnActivateWindow();
  void OnCloseWindow();
  void OnRequestDockWindow();
  void OnRequestUndockWindow();
  void OnRuntimePropertyChanged(const std::string& name,
                                const std::string& value);

  DISALLOW_COPY_AND_ASSIGN(DevToolsTabHelper);
};

#endif  // CHROME_BROWSER_DEBUGGER_DEVTOOLS_TAB_HELPER_H_
