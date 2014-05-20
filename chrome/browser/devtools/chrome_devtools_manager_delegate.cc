// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"

#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/devtools_agent_host.h"

ChromeDevToolsManagerDelegate::ChromeDevToolsManagerDelegate() {
}

ChromeDevToolsManagerDelegate::~ChromeDevToolsManagerDelegate() {
}

void ChromeDevToolsManagerDelegate::Inspect(
    content::BrowserContext* browser_context,
    content::DevToolsAgentHost* agent_host) {
  if (!agent_host->IsWorker()) {
    // TODO(horo): Support other types of DevToolsAgentHost when necessary.
    NOTREACHED() << "Inspect() only supports workers.";
  }
#if !defined(OS_ANDROID)
  if (Profile* profile = Profile::FromBrowserContext(browser_context))
    DevToolsWindow::OpenDevToolsWindowForWorker(profile, agent_host);
#endif
}

base::DictionaryValue* ChromeDevToolsManagerDelegate::HandleCommand(
    content::DevToolsAgentHost* agent_host,
    base::DictionaryValue* command_dict) {
  return NULL;
}
