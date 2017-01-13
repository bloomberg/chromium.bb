// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_devtools_manager_delegate.h"

#include <string>
#include <utility>

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_frontend_host.h"
#include "content/public/browser/web_contents.h"
#include "headless/grit/headless_lib_resources.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/public/devtools/domains/target.h"
#include "ui/base/resource/resource_bundle.h"

namespace headless {

namespace {
const char kIdParam[] = "id";
const char kResultParam[] = "result";
const char kErrorParam[] = "error";
const char kErrorCodeParam[] = "code";
const char kErrorMessageParam[] = "message";

// JSON RPC 2.0 spec: http://www.jsonrpc.org/specification#error_object
enum Error {
  kErrorInvalidParam = -32602,
  kErrorServerError = -32000
};

std::unique_ptr<base::DictionaryValue> CreateSuccessResponse(
    int command_id,
    std::unique_ptr<base::Value> result) {
  if (!result)
    result.reset(new base::DictionaryValue());

  std::unique_ptr<base::DictionaryValue> response(new base::DictionaryValue());
  response->SetInteger(kIdParam, command_id);
  response->Set(kResultParam, std::move(result));
  return response;
}

std::unique_ptr<base::DictionaryValue> CreateErrorResponse(
    int command_id,
    int error_code,
    const std::string& error_message) {
  std::unique_ptr<base::DictionaryValue> error_object(
      new base::DictionaryValue());
  error_object->SetInteger(kErrorCodeParam, error_code);
  error_object->SetString(kErrorMessageParam, error_message);

  std::unique_ptr<base::DictionaryValue> response(new base::DictionaryValue());
  response->Set(kErrorParam, std::move(error_object));
  return response;
}

std::unique_ptr<base::DictionaryValue> CreateInvalidParamResponse(
    int command_id,
    const std::string& param) {
  return CreateErrorResponse(
      command_id, kErrorInvalidParam,
      base::StringPrintf("Missing or invalid '%s' parameter", param.c_str()));
}
}  // namespace

HeadlessDevToolsManagerDelegate::HeadlessDevToolsManagerDelegate(
    base::WeakPtr<HeadlessBrowserImpl> browser)
    : browser_(std::move(browser)) {
  command_map_["Target.createTarget"] =
      &HeadlessDevToolsManagerDelegate::CreateTarget;
  command_map_["Target.closeTarget"] =
      &HeadlessDevToolsManagerDelegate::CloseTarget;
  command_map_["Target.createBrowserContext"] =
      &HeadlessDevToolsManagerDelegate::CreateBrowserContext;
  command_map_["Target.disposeBrowserContext"] =
      &HeadlessDevToolsManagerDelegate::DisposeBrowserContext;
}

HeadlessDevToolsManagerDelegate::~HeadlessDevToolsManagerDelegate() {}

base::DictionaryValue* HeadlessDevToolsManagerDelegate::HandleCommand(
    content::DevToolsAgentHost* agent_host,
    base::DictionaryValue* command) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!browser_)
    return nullptr;

  int id;
  std::string method;
  if (!command->GetInteger("id", &id) ||
      !command->GetString("method", &method)) {
    return nullptr;
  }
  auto find_it = command_map_.find(method);
  if (find_it == command_map_.end())
    return nullptr;
  CommandMemberFnPtr command_fn_ptr = find_it->second;
  const base::DictionaryValue* params = nullptr;
  command->GetDictionary("params", &params);
  std::unique_ptr<base::DictionaryValue> cmd_result(
      ((this)->*command_fn_ptr)(id, params));
  return cmd_result.release();
}

std::string HeadlessDevToolsManagerDelegate::GetDiscoveryPageHTML() {
  return ResourceBundle::GetSharedInstance()
      .GetRawDataResource(IDR_HEADLESS_LIB_DEVTOOLS_DISCOVERY_PAGE)
      .as_string();
}

std::string HeadlessDevToolsManagerDelegate::GetFrontendResource(
    const std::string& path) {
  return content::DevToolsFrontendHost::GetFrontendResource(path).as_string();
}

std::unique_ptr<base::DictionaryValue>
HeadlessDevToolsManagerDelegate::CreateTarget(
    int command_id,
    const base::DictionaryValue* params) {
  std::string url;
  std::string browser_context_id;
  int width = browser_->options()->window_size.width();
  int height = browser_->options()->window_size.height();
  if (!params || !params->GetString("url", &url))
    return CreateInvalidParamResponse(command_id, "url");
  params->GetString("browserContextId", &browser_context_id);
  params->GetInteger("width", &width);
  params->GetInteger("height", &height);

  HeadlessBrowserContext* context =
      browser_->GetBrowserContextForId(browser_context_id);
  if (!browser_context_id.empty()) {
    context = browser_->GetBrowserContextForId(browser_context_id);
    if (!context)
      return CreateInvalidParamResponse(command_id, "browserContextId");
  } else {
    context = browser_->GetDefaultBrowserContext();
    if (!context) {
      return CreateErrorResponse(command_id, kErrorServerError,
                                 "You specified no |browserContextId|, but "
                                 "there is no default browser context set on "
                                 "HeadlessBrowser");
    }
  }

  HeadlessWebContentsImpl* web_contents_impl =
      HeadlessWebContentsImpl::From(context->CreateWebContentsBuilder()
                                        .SetInitialURL(GURL(url))
                                        .SetWindowSize(gfx::Size(width, height))
                                        .Build());

  std::unique_ptr<base::Value> result(
      target::CreateTargetResult::Builder()
          .SetTargetId(web_contents_impl->GetDevToolsAgentHostId())
          .Build()
          ->Serialize());
  return CreateSuccessResponse(command_id, std::move(result));
}

std::unique_ptr<base::DictionaryValue>
HeadlessDevToolsManagerDelegate::CloseTarget(
    int command_id,
    const base::DictionaryValue* params) {
  std::string target_id;
  if (!params || !params->GetString("targetId", &target_id))
    return CreateInvalidParamResponse(command_id, "targetId");
  HeadlessWebContents* web_contents =
      browser_->GetWebContentsForDevToolsAgentHostId(target_id);
  bool success = false;
  if (web_contents) {
    web_contents->Close();
    success = true;
  }
  std::unique_ptr<base::Value> result(target::CloseTargetResult::Builder()
                                          .SetSuccess(success)
                                          .Build()
                                          ->Serialize());
  return CreateSuccessResponse(command_id, std::move(result));
}

std::unique_ptr<base::DictionaryValue>
HeadlessDevToolsManagerDelegate::CreateBrowserContext(
    int command_id,
    const base::DictionaryValue* params) {
  HeadlessBrowserContext* browser_context =
      browser_->CreateBrowserContextBuilder().Build();

  std::unique_ptr<base::Value> result(
      target::CreateBrowserContextResult::Builder()
          .SetBrowserContextId(browser_context->Id())
          .Build()
          ->Serialize());
  return CreateSuccessResponse(command_id, std::move(result));
}

std::unique_ptr<base::DictionaryValue>
HeadlessDevToolsManagerDelegate::DisposeBrowserContext(
    int command_id,
    const base::DictionaryValue* params) {
  std::string browser_context_id;
  if (!params || !params->GetString("browserContextId", &browser_context_id))
    return CreateInvalidParamResponse(command_id, "browserContextId");
  HeadlessBrowserContext* context =
      browser_->GetBrowserContextForId(browser_context_id);

  bool success = false;
  if (context && context != browser_->GetDefaultBrowserContext() &&
      context->GetAllWebContents().empty()) {
    success = true;
    context->Close();
  }

  std::unique_ptr<base::Value> result(
      target::DisposeBrowserContextResult::Builder()
          .SetSuccess(success)
          .Build()
          ->Serialize());
  return CreateSuccessResponse(command_id, std::move(result));
}

}  // namespace headless
