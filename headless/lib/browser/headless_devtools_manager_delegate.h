// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_DEVTOOLS_MANAGER_DELEGATE_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_DEVTOOLS_MANAGER_DELEGATE_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "printing/features/features.h"

#if BUILDFLAG(ENABLE_BASIC_PRINTING)
#include "headless/lib/browser/headless_print_manager.h"
#include "headless/public/headless_export.h"
#endif

namespace headless {
class HeadlessBrowserImpl;

#if BUILDFLAG(ENABLE_BASIC_PRINTING)
// Exported for tests.
HEADLESS_EXPORT std::unique_ptr<base::DictionaryValue> ParsePrintSettings(
    int command_id,
    const base::DictionaryValue* params,
    HeadlessPrintSettings* settings);
#endif

class HeadlessDevToolsManagerDelegate
    : public content::DevToolsManagerDelegate {
 public:
  explicit HeadlessDevToolsManagerDelegate(
      base::WeakPtr<HeadlessBrowserImpl> browser);
  ~HeadlessDevToolsManagerDelegate() override;

  // DevToolsManagerDelegate implementation:
  bool HandleCommand(content::DevToolsAgentHost* agent_host,
                     int session_id,
                     base::DictionaryValue* command) override;
  bool HandleAsyncCommand(content::DevToolsAgentHost* agent_host,
                          int session_id,
                          base::DictionaryValue* command,
                          const CommandCallback& callback) override;
  scoped_refptr<content::DevToolsAgentHost> CreateNewTarget(
      const GURL& url) override;
  std::string GetDiscoveryPageHTML() override;
  std::string GetFrontendResource(const std::string& path) override;

 private:
  std::unique_ptr<base::DictionaryValue> CreateTarget(
      int command_id,
      const base::DictionaryValue* params);
  std::unique_ptr<base::DictionaryValue> CloseTarget(
      int command_id,
      const base::DictionaryValue* params);
  std::unique_ptr<base::DictionaryValue> CreateBrowserContext(
      int command_id,
      const base::DictionaryValue* params);
  std::unique_ptr<base::DictionaryValue> DisposeBrowserContext(
      int command_id,
      const base::DictionaryValue* params);
  std::unique_ptr<base::DictionaryValue> GetWindowForTarget(
      int command_id,
      const base::DictionaryValue* params);
  std::unique_ptr<base::DictionaryValue> GetWindowBounds(
      int command_id,
      const base::DictionaryValue* params);
  std::unique_ptr<base::DictionaryValue> SetWindowBounds(
      int command_id,
      const base::DictionaryValue* params);
  std::unique_ptr<base::DictionaryValue> EmulateNetworkConditions(
      int command_id,
      const base::DictionaryValue* params);
  std::unique_ptr<base::DictionaryValue> NetworkDisable(
      int command_id,
      const base::DictionaryValue* params);

  void PrintToPDF(content::DevToolsAgentHost* agent_host,
                  int command_id,
                  const base::DictionaryValue* params,
                  const CommandCallback& callback);

  base::WeakPtr<HeadlessBrowserImpl> browser_;

  using CommandMemberCallback =
      base::Callback<std::unique_ptr<base::DictionaryValue>(
          int command_id,
          const base::DictionaryValue* params)>;
  using AsyncCommandMemberCallback =
      base::Callback<void(content::DevToolsAgentHost* agent_host,
                          int command_id,
                          const base::DictionaryValue* params,
                          const CommandCallback& callback)>;
  std::map<std::string, CommandMemberCallback> command_map_;
  std::map<std::string, AsyncCommandMemberCallback> async_command_map_;

  // These commands are passed on for devtools to handle.
  std::map<std::string, CommandMemberCallback> unhandled_command_map_;
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_DEVTOOLS_MANAGER_DELEGATE_H_
