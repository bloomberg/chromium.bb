// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_devtools_manager_delegate.h"

#include <string>

#include "base/guid.h"
#include "content/public/browser/web_contents.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/public/domains/browser.h"

namespace headless {

HeadlessDevToolsManagerDelegate::HeadlessDevToolsManagerDelegate(
    HeadlessBrowserImpl* browser)
    : browser_(browser) {
  command_map_["Browser.createTarget"] =
      &HeadlessDevToolsManagerDelegate::CreateTarget;
  command_map_["Browser.closeTarget"] =
      &HeadlessDevToolsManagerDelegate::CloseTarget;
  command_map_["Browser.createBrowserContext"] =
      &HeadlessDevToolsManagerDelegate::CreateBrowserContext;
  command_map_["Browser.disposeBrowserContext"] =
      &HeadlessDevToolsManagerDelegate::DisposeBrowserContext;
}

HeadlessDevToolsManagerDelegate::~HeadlessDevToolsManagerDelegate() {}

base::DictionaryValue* HeadlessDevToolsManagerDelegate::HandleCommand(
    content::DevToolsAgentHost* agent_host,
    base::DictionaryValue* command) {
  int id;
  std::string method;
  const base::DictionaryValue* params = nullptr;
  if (!command->GetInteger("id", &id) ||
      !command->GetString("method", &method) ||
      !command->GetDictionary("params", &params)) {
    return nullptr;
  }
  auto find_it = command_map_.find(method);
  if (find_it == command_map_.end())
    return nullptr;
  CommandMemberFnPtr command_fn_ptr = find_it->second;
  std::unique_ptr<base::Value> cmd_result(((this)->*command_fn_ptr)(params));
  if (!cmd_result)
    return nullptr;

  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  result->SetInteger("id", id);
  result->Set("result", std::move(cmd_result));
  return result.release();
}

std::unique_ptr<base::Value> HeadlessDevToolsManagerDelegate::CreateTarget(
    const base::DictionaryValue* params) {
  std::string initial_url;
  std::string browser_context_id;
  int width = 800;
  int height = 600;
  params->GetString("initialUrl", &initial_url);
  params->GetString("browserContextId", &browser_context_id);
  params->GetInteger("width", &width);
  params->GetInteger("height", &height);
  HeadlessWebContentsImpl* web_contents_impl;
  auto find_it = browser_context_map_.find(browser_context_id);
  if (find_it != browser_context_map_.end()) {
    web_contents_impl = HeadlessWebContentsImpl::From(
        browser_->CreateWebContentsBuilder()
            .SetInitialURL(GURL(initial_url))
            .SetWindowSize(gfx::Size(width, height))
            .SetBrowserContext(find_it->second.get())
            .Build());
  } else {
    web_contents_impl = HeadlessWebContentsImpl::From(
        browser_->CreateWebContentsBuilder()
            .SetInitialURL(GURL(initial_url))
            .SetWindowSize(gfx::Size(width, height))
            .Build());
  }
  return browser::CreateTargetResult::Builder()
      .SetTargetId(web_contents_impl->GetDevtoolsAgentHostId())
      .Build()
      ->Serialize();
}

std::unique_ptr<base::Value> HeadlessDevToolsManagerDelegate::CloseTarget(
    const base::DictionaryValue* params) {
  std::string target_id;
  if (!params->GetString("targetId", &target_id)) {
    return nullptr;
  }
  HeadlessWebContents* web_contents =
      browser_->GetWebContentsForDevtoolsAgentHostId(target_id);
  bool success = false;
  if (web_contents) {
    web_contents->Close();
    success = true;
  }
  return browser::CloseTargetResult::Builder()
      .SetSuccess(success)
      .Build()
      ->Serialize();
}

std::unique_ptr<base::Value>
HeadlessDevToolsManagerDelegate::CreateBrowserContext(
    const base::DictionaryValue* params) {
  std::string browser_context_id = base::GenerateGUID();
  browser_context_map_[browser_context_id] =
      browser_->CreateBrowserContextBuilder().Build();
  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  return browser::CreateBrowserContextResult::Builder()
      .SetBrowserContextId(browser_context_id)
      .Build()
      ->Serialize();
}

std::unique_ptr<base::Value>
HeadlessDevToolsManagerDelegate::DisposeBrowserContext(
    const base::DictionaryValue* params) {
  std::string browser_context_id;
  if (!params->GetString("browserContextId", &browser_context_id)) {
    return nullptr;
  }
  auto find_it = browser_context_map_.find(browser_context_id);
  bool success = false;
  if (find_it != browser_context_map_.end()) {
    success = true;
    HeadlessBrowserContextImpl* headless_browser_context =
        HeadlessBrowserContextImpl::From(find_it->second.get());
    // Make sure |headless_browser_context| isn't in use!
    for (HeadlessWebContents* headless_web_contents :
         browser_->GetAllWebContents()) {
      content::WebContents* web_contents =
          HeadlessWebContentsImpl::From(headless_web_contents)->web_contents();
      if (web_contents->GetBrowserContext() == headless_browser_context) {
        success = false;
        break;
      }
    }
    if (success)
      browser_context_map_.erase(find_it);
  }
  return browser::DisposeBrowserContextResult::Builder()
      .SetSuccess(success)
      .Build()
      ->Serialize();
}

}  // namespace headless
