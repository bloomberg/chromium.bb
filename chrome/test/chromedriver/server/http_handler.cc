// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/server/http_handler.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "chrome/test/chromedriver/command_executor.h"
#include "chrome/test/chromedriver/command_names.h"
#include "chrome/test/chromedriver/server/http_response.h"
#include "chrome/test/chromedriver/status.h"
#include "chrome/test/chromedriver/util.h"

namespace {

const char kShutdownPath[] = "shutdown";

}  // namespace

HttpRequest::HttpRequest(HttpMethod method,
                         const std::string& path,
                         const std::string& body)
    : method(method), path(path), body(body) {}

HttpRequest::~HttpRequest() {}

CommandMapping::CommandMapping(HttpMethod method,
                               const std::string& path_pattern,
                               const std::string& name)
    : method(method), path_pattern(path_pattern), name(name) {}

CommandMapping::~CommandMapping() {}

// static
scoped_ptr<HttpHandler::CommandMap> HttpHandler::CreateCommandMap() {
  CommandMapping commands[] = {
      CommandMapping(kPost, "session", internal::kNewSessionIdCommand),
      CommandMapping(kGet, "session/:sessionId", CommandNames::kNewSession),
      CommandMapping(kDelete, "session/:sessionId", CommandNames::kQuit),
      CommandMapping(kGet, "session/:sessionId/window_handle",
                     CommandNames::kGetCurrentWindowHandle),
      CommandMapping(kGet, "session/:sessionId/window_handles",
                     CommandNames::kGetWindowHandles),
      CommandMapping(kPost, "session/:sessionId/url", CommandNames::kGet),
      CommandMapping(kGet, "session/:sessionId/alert", CommandNames::kGetAlert),
      CommandMapping(kPost, "session/:sessionId/dismiss_alert",
                     CommandNames::kDismissAlert),
      CommandMapping(kPost, "session/:sessionId/accept_alert",
                     CommandNames::kAcceptAlert),
      CommandMapping(kGet, "session/:sessionId/alert_text",
                     CommandNames::kGetAlertText),
      CommandMapping(kPost, "session/:sessionId/alert_text",
                     CommandNames::kSetAlertValue),
      CommandMapping(kPost, "session/:sessionId/forward",
                     CommandNames::kGoForward),
      CommandMapping(kPost, "session/:sessionId/back", CommandNames::kGoBack),
      CommandMapping(kPost, "session/:sessionId/refresh",
                     CommandNames::kRefresh),
      CommandMapping(kPost, "session/:sessionId/execute",
                     CommandNames::kExecuteScript),
      CommandMapping(kPost, "session/:sessionId/execute_async",
                     CommandNames::kExecuteAsyncScript),
      CommandMapping(kGet, "session/:sessionId/url",
                     CommandNames::kGetCurrentUrl),
      CommandMapping(kGet, "session/:sessionId/title", CommandNames::kGetTitle),
      CommandMapping(kGet, "session/:sessionId/source",
                     CommandNames::kGetPageSource),
      CommandMapping(kGet, "session/:sessionId/screenshot",
                     CommandNames::kScreenshot),
      CommandMapping(kPost, "session/:sessionId/visible",
                     CommandNames::kSetBrowserVisible),
      CommandMapping(kGet, "session/:sessionId/visible",
                     CommandNames::kIsBrowserVisible),
      CommandMapping(kPost, "session/:sessionId/element",
                     CommandNames::kFindElement),
      CommandMapping(kPost, "session/:sessionId/elements",
                     CommandNames::kFindElements),
      CommandMapping(kPost, "session/:sessionId/element/active",
                     CommandNames::kGetActiveElement),
      CommandMapping(kPost, "session/:sessionId/element/:id/element",
                     CommandNames::kFindChildElement),
      CommandMapping(kPost, "session/:sessionId/element/:id/elements",
                     CommandNames::kFindChildElements),
      CommandMapping(kPost, "session/:sessionId/element/:id/click",
                     CommandNames::kClickElement),
      CommandMapping(kPost, "session/:sessionId/element/:id/clear",
                     CommandNames::kClearElement),
      CommandMapping(kPost, "session/:sessionId/element/:id/submit",
                     CommandNames::kSubmitElement),
      CommandMapping(kGet, "session/:sessionId/element/:id/text",
                     CommandNames::kGetElementText),
      CommandMapping(kPost, "session/:sessionId/element/:id/value",
                     CommandNames::kSendKeysToElement),
      CommandMapping(kPost, "session/:sessionId/file",
                     CommandNames::kUploadFile),
      CommandMapping(kGet, "session/:sessionId/element/:id/value",
                     CommandNames::kGetElementValue),
      CommandMapping(kGet, "session/:sessionId/element/:id/name",
                     CommandNames::kGetElementTagName),
      CommandMapping(kGet, "session/:sessionId/element/:id/selected",
                     CommandNames::kIsElementSelected),
      CommandMapping(kGet, "session/:sessionId/element/:id/enabled",
                     CommandNames::kIsElementEnabled),
      CommandMapping(kGet, "session/:sessionId/element/:id/displayed",
                     CommandNames::kIsElementDisplayed),
      CommandMapping(kPost, "session/:sessionId/element/:id/hover",
                     CommandNames::kHoverOverElement),
      CommandMapping(kGet, "session/:sessionId/element/:id/location",
                     CommandNames::kGetElementLocation),
      CommandMapping(kGet, "session/:sessionId/element/:id/location_in_view",
                     CommandNames::kGetElementLocationOnceScrolledIntoView),
      CommandMapping(kGet, "session/:sessionId/element/:id/size",
                     CommandNames::kGetElementSize),
      CommandMapping(kGet, "session/:sessionId/element/:id/attribute/:name",
                     CommandNames::kGetElementAttribute),
      CommandMapping(kGet, "session/:sessionId/element/:id/equals/:other",
                     CommandNames::kElementEquals),
      CommandMapping(kGet, "session/:sessionId/cookie",
                     CommandNames::kGetCookies),
      CommandMapping(kPost, "session/:sessionId/cookie",
                     CommandNames::kAddCookie),
      CommandMapping(kDelete, "session/:sessionId/cookie",
                     CommandNames::kDeleteAllCookies),
      CommandMapping(kDelete, "session/:sessionId/cookie/:name",
                     CommandNames::kDeleteCookie),
      CommandMapping(kPost, "session/:sessionId/frame",
                     CommandNames::kSwitchToFrame),
      CommandMapping(kPost, "session/:sessionId/window",
                     CommandNames::kSwitchToWindow),
      CommandMapping(kGet, "session/:sessionId/window/:windowHandle/size",
                     CommandNames::kGetWindowSize),
      CommandMapping(kGet, "session/:sessionId/window/:windowHandle/position",
                     CommandNames::kGetWindowPosition),
      CommandMapping(kPost, "session/:sessionId/window/:windowHandle/size",
                     CommandNames::kSetWindowSize),
      CommandMapping(kPost, "session/:sessionId/window/:windowHandle/position",
                     CommandNames::kSetWindowPosition),
      CommandMapping(kPost, "session/:sessionId/window/:windowHandle/maximize",
                     CommandNames::kMaximizeWindow),
      CommandMapping(kDelete, "session/:sessionId/window",
                     CommandNames::kClose),
      CommandMapping(kPost, "session/:sessionId/element/:id/drag",
                     CommandNames::kDragElement),
      CommandMapping(kGet, "session/:sessionId/element/:id/css/:propertyName",
                     CommandNames::kGetElementValueOfCssProperty),
      CommandMapping(kPost, "session/:sessionId/timeouts/implicit_wait",
                     CommandNames::kImplicitlyWait),
      CommandMapping(kPost, "session/:sessionId/timeouts/async_script",
                     CommandNames::kSetScriptTimeout),
      CommandMapping(kPost, "session/:sessionId/timeouts",
                     CommandNames::kSetTimeout),
      CommandMapping(kPost, "session/:sessionId/execute_sql",
                     CommandNames::kExecuteSQL),
      CommandMapping(kGet, "session/:sessionId/location",
                     CommandNames::kGetLocation),
      CommandMapping(kPost, "session/:sessionId/location",
                     CommandNames::kSetLocation),
      CommandMapping(kGet, "session/:sessionId/application_cache/status",
                     CommandNames::kGetStatus),
      CommandMapping(kGet, "session/:sessionId/browser_connection",
                     CommandNames::kIsBrowserOnline),
      CommandMapping(kPost, "session/:sessionId/browser_connection",
                     CommandNames::kSetBrowserOnline),
      CommandMapping(kGet, "session/:sessionId/local_storage/key/:key",
                     CommandNames::kGetLocalStorageItem),
      CommandMapping(kDelete, "session/:sessionId/local_storage/key/:key",
                     CommandNames::kRemoveLocalStorageItem),
      CommandMapping(kGet, "session/:sessionId/local_storage",
                     CommandNames::kGetLocalStorageKeys),
      CommandMapping(kPost, "session/:sessionId/local_storage",
                     CommandNames::kSetLocalStorageItem),
      CommandMapping(kDelete, "session/:sessionId/local_storage",
                     CommandNames::kClearLocalStorage),
      CommandMapping(kGet, "session/:sessionId/local_storage/size",
                     CommandNames::kGetLocalStorageSize),
      CommandMapping(kGet, "session/:sessionId/session_storage/key/:key",
                     CommandNames::kGetSessionStorageItem),
      CommandMapping(kDelete, "session/:sessionId/session_storage/key/:key",
                     CommandNames::kRemoveSessionStorageItem),
      CommandMapping(kGet, "session/:sessionId/session_storage",
                     CommandNames::kGetSessionStorageKey),
      CommandMapping(kPost, "session/:sessionId/session_storage",
                     CommandNames::kSetSessionStorageItem),
      CommandMapping(kDelete, "session/:sessionId/session_storage",
                     CommandNames::kClearSessionStorage),
      CommandMapping(kGet, "session/:sessionId/session_storage/size",
                     CommandNames::kGetSessionStorageSize),
      CommandMapping(kGet, "session/:sessionId/orientation",
                     CommandNames::kGetScreenOrientation),
      CommandMapping(kPost, "session/:sessionId/orientation",
                     CommandNames::kSetScreenOrientation),
      CommandMapping(kPost, "session/:sessionId/click",
                     CommandNames::kMouseClick),
      CommandMapping(kPost, "session/:sessionId/doubleclick",
                     CommandNames::kMouseDoubleClick),
      CommandMapping(kPost, "session/:sessionId/buttondown",
                     CommandNames::kMouseButtonDown),
      CommandMapping(kPost, "session/:sessionId/buttonup",
                     CommandNames::kMouseButtonUp),
      CommandMapping(kPost, "session/:sessionId/moveto",
                     CommandNames::kMouseMoveTo),
      CommandMapping(kPost, "session/:sessionId/keys",
                     CommandNames::kSendKeysToActiveElement),
      CommandMapping(kGet, "session/:sessionId/ime/available_engines",
                     CommandNames::kImeGetAvailableEngines),
      CommandMapping(kGet, "session/:sessionId/ime/active_engine",
                     CommandNames::kImeGetActiveEngine),
      CommandMapping(kGet, "session/:sessionId/ime/activated",
                     CommandNames::kImeIsActivated),
      CommandMapping(kPost, "session/:sessionId/ime/deactivate",
                     CommandNames::kImeDeactivate),
      CommandMapping(kPost, "session/:sessionId/ime/activate",
                     CommandNames::kImeActivateEngine),
      CommandMapping(kPost, "session/:sessionId/touch/click",
                     CommandNames::kTouchSingleTap),
      CommandMapping(kPost, "session/:sessionId/touch/down",
                     CommandNames::kTouchDown),
      CommandMapping(kPost, "session/:sessionId/touch/up",
                     CommandNames::kTouchUp),
      CommandMapping(kPost, "session/:sessionId/touch/move",
                     CommandNames::kTouchMove),
      CommandMapping(kPost, "session/:sessionId/touch/scroll",
                     CommandNames::kTouchScroll),
      CommandMapping(kPost, "session/:sessionId/touch/doubleclick",
                     CommandNames::kTouchDoubleTap),
      CommandMapping(kPost, "session/:sessionId/touch/longclick",
                     CommandNames::kTouchLongPress),
      CommandMapping(kPost, "session/:sessionId/touch/flick",
                     CommandNames::kTouchFlick),
      CommandMapping(kPost, "session/:sessionId/log", CommandNames::kGetLog),
      CommandMapping(kGet, "session/:sessionId/log/types",
                     CommandNames::kGetAvailableLogTypes),
      CommandMapping(kPost, "logs", CommandNames::kGetSessionLogs),
      CommandMapping(kGet, "status", CommandNames::kStatus),

      // Custom Chrome commands:
      // Allow quit all to be called with GET or POST.
      CommandMapping(kGet, kShutdownPath, CommandNames::kQuitAll),
      CommandMapping(kPost, kShutdownPath, CommandNames::kQuitAll),
  };
  return scoped_ptr<CommandMap>(
      new CommandMap(commands, commands + arraysize(commands)));
}

HttpHandler::HttpHandler(scoped_ptr<CommandExecutor> executor,
                         scoped_ptr<CommandMap> command_map,
                         const std::string& url_base)
    : executor_(executor.Pass()),
      command_map_(command_map.Pass()),
      url_base_(url_base) {
  executor_->Init();
}

HttpHandler::~HttpHandler() {}

void HttpHandler::Handle(const HttpRequest& request,
                         HttpResponse* response) {
  std::string path = request.path;
  if (!StartsWithASCII(path, url_base_, true)) {
    *response = HttpResponse(HttpResponse::kBadRequest);
    response->set_body("unhandled request");
    return;
  }

  path.erase(0, url_base_.length());

  if (!HandleWebDriverCommand(request, path, response)) {
    *response = HttpResponse(HttpResponse::kNotFound);
    response->set_body("unknown command: " + path);
    return;
  }
}

bool HttpHandler::ShouldShutdown(const HttpRequest& request) {
  return request.path == url_base_ + kShutdownPath;
}

bool HttpHandler::HandleWebDriverCommand(
    const HttpRequest& request,
    const std::string& trimmed_path,
    HttpResponse* response) {
  base::DictionaryValue params;
  std::string session_id;
  CommandMap::const_iterator iter = command_map_->begin();
  while (true) {
    if (iter == command_map_->end()) {
      return false;
    }
    if (internal::MatchesCommand(
            request.method, trimmed_path, *iter, &session_id, &params)) {
      break;
    }
    ++iter;
  }

  if (iter->name == internal::kNewSessionIdCommand) {
    // Creating a session involves a HTTP request to /session, which is
    // supposed to redirect to /session/:sessionId, which returns the
    // session info. Thus, the create new session command actually is split
    // between two HTTP requests. For this first request, we just redirect
    // to a new unique session ID. On the second request the create new
    // session command will be invoked.
    *response = HttpResponse(HttpResponse::kSeeOther);
    response->AddHeader("Location", url_base_ + "session/" + GenerateId());
    return true;
  }

  if (request.method == kPost && request.body.length()) {
    base::DictionaryValue* body_params;
    scoped_ptr<base::Value> parsed_body(base::JSONReader::Read(request.body));
    if (!parsed_body || !parsed_body->GetAsDictionary(&body_params)) {
      *response = HttpResponse(HttpResponse::kBadRequest);
      response->set_body("missing command parameters");
      return true;
    }
    params.MergeDictionary(body_params);
  }

  StatusCode status = kOk;
  scoped_ptr<base::Value> value;
  std::string out_session_id;
  executor_->ExecuteCommand(
      iter->name, params, session_id, &status, &value, &out_session_id);

  if (status == kUnknownCommand) {
    *response = HttpResponse(HttpResponse::kNotImplemented);
    response->set_body("unimplemented command: " + iter->name);
    return true;
  }

  base::DictionaryValue body_params;
  body_params.SetInteger("status", status);
  body_params.Set("value", value.release());
  body_params.SetString("sessionId", out_session_id);
  std::string body;
  base::JSONWriter::Write(&body_params, &body);
  *response = HttpResponse(HttpResponse::kOk);
  response->SetMimeType("application/json; charset=utf-8");
  response->set_body(body);
  return true;
}

namespace internal {

const char kNewSessionIdCommand[] = "chromedriver:newSessionIdCommand";

bool MatchesCommand(HttpMethod method,
                    const std::string& path,
                    const CommandMapping& command,
                    std::string* session_id,
                    base::DictionaryValue* out_params) {
  if (method != command.method)
    return false;

  std::vector<std::string> path_parts;
  base::SplitString(path, '/', &path_parts);
  std::vector<std::string> command_path_parts;
  base::SplitString(command.path_pattern, '/', &command_path_parts);
  if (path_parts.size() != command_path_parts.size())
    return false;

  base::DictionaryValue params;
  for (size_t i = 0; i < path_parts.size(); ++i) {
    CHECK(command_path_parts[i].length());
    if (command_path_parts[i][0] == ':') {
      std::string name = command_path_parts[i];
      name.erase(0, 1);
      CHECK(name.length());
      if (name == "sessionId")
        *session_id = path_parts[i];
      else
        params.SetString(name, path_parts[i]);
    } else if (command_path_parts[i] != path_parts[i]) {
      return false;
    }
  }
  out_params->MergeDictionary(&params);
  return true;
}

}  // namespace
