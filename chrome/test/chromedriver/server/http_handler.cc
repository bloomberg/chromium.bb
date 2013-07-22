// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/server/http_handler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"  // For CHECK macros.
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "chrome/test/chromedriver/alert_commands.h"
#include "chrome/test/chromedriver/chrome/adb_impl.h"
#include "chrome/test/chromedriver/chrome/device_manager.h"
#include "chrome/test/chromedriver/chrome/log.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/version.h"
#include "chrome/test/chromedriver/commands.h"
#include "chrome/test/chromedriver/element_commands.h"
#include "chrome/test/chromedriver/net/url_request_context_getter.h"
#include "chrome/test/chromedriver/server/http_response.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/session_commands.h"
#include "chrome/test/chromedriver/session_map.h"
#include "chrome/test/chromedriver/util.h"
#include "chrome/test/chromedriver/window_commands.h"
#include "net/server/http_server_request_info.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

namespace {

const char kLocalStorage[] = "localStorage";
const char kSessionStorage[] = "sessionStorage";
const char kShutdownPath[] = "shutdown";

Status UnimplementedCommand(
    const base::DictionaryValue& params,
    const std::string& session_id,
    scoped_ptr<base::Value>* value,
    std::string* out_session_id) {
  return Status(kUnknownCommand);
}

}  // namespace

CommandMapping::CommandMapping(HttpMethod method,
                               const std::string& path_pattern,
                               const Command& command)
    : method(method), path_pattern(path_pattern), command(command) {}

CommandMapping::~CommandMapping() {}

HttpHandler::HttpHandler(Log* log, const std::string& url_base)
    : log_(log),
      io_thread_("ChromeDriver IO"),
      url_base_(url_base) {
#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool autorelease_pool;
#endif
  base::Thread::Options options(base::MessageLoop::TYPE_IO, 0);
  CHECK(io_thread_.StartWithOptions(options));
  context_getter_ = new URLRequestContextGetter(
      io_thread_.message_loop_proxy());
  socket_factory_ = CreateSyncWebSocketFactory(context_getter_.get());
  adb_.reset(new AdbImpl(io_thread_.message_loop_proxy(), log_));
  device_manager_.reset(new DeviceManager(adb_.get()));

  CommandMapping commands[] = {
      CommandMapping(kPost, internal::kNewSessionPathPattern,
                     base::Bind(&ExecuteNewSession,
                                NewSessionParams(log_, &session_map_,
                                    context_getter_, socket_factory_,
                                    device_manager_.get()))),
      CommandMapping(kGet, "session/:sessionId",
                     WrapToCommand(
                         base::Bind(&ExecuteGetSessionCapabilities,
                             &session_map_))),
      CommandMapping(kDelete, "session/:sessionId",
                     base::Bind(&ExecuteQuit, false, &session_map_)),
      CommandMapping(kGet, "session/:sessionId/window_handle",
                     WrapToCommand(base::Bind(&ExecuteGetCurrentWindowHandle))),
      CommandMapping(kGet, "session/:sessionId/window_handles",
                     WrapToCommand(base::Bind(&ExecuteGetWindowHandles))),
      CommandMapping(kPost, "session/:sessionId/url",
                     WrapToCommand(base::Bind(&ExecuteGet))),
      CommandMapping(kGet, "session/:sessionId/alert",
                     WrapToCommand(
                         base::Bind(&ExecuteAlertCommand,
                                     base::Bind(&ExecuteGetAlert)))),
      CommandMapping(kPost, "session/:sessionId/dismiss_alert",
                     WrapToCommand(
                         base::Bind(&ExecuteAlertCommand,
                                    base::Bind(&ExecuteDismissAlert)))),
      CommandMapping(kPost, "session/:sessionId/accept_alert",
                     WrapToCommand(
                         base::Bind(&ExecuteAlertCommand,
                                    base::Bind(&ExecuteAcceptAlert)))),
      CommandMapping(kGet, "session/:sessionId/alert_text",
                     WrapToCommand(
                         base::Bind(&ExecuteAlertCommand,
                                    base::Bind(&ExecuteGetAlertText)))),
      CommandMapping(kPost, "session/:sessionId/alert_text",
                     WrapToCommand(
                         base::Bind(&ExecuteAlertCommand,
                                    base::Bind(&ExecuteSetAlertValue)))),
      CommandMapping(kPost, "session/:sessionId/forward",
                     WrapToCommand(base::Bind(&ExecuteGoForward))),
      CommandMapping(kPost, "session/:sessionId/back",
                     WrapToCommand(base::Bind(&ExecuteGoBack))),
      CommandMapping(kPost, "session/:sessionId/refresh",
                     WrapToCommand(base::Bind(&ExecuteRefresh))),
      CommandMapping(kPost, "session/:sessionId/execute",
                     WrapToCommand(base::Bind(&ExecuteExecuteScript))),
      CommandMapping(kPost, "session/:sessionId/execute_async",
                     WrapToCommand(base::Bind(&ExecuteExecuteAsyncScript))),
      CommandMapping(kGet, "session/:sessionId/url",
                     WrapToCommand(base::Bind(&ExecuteGetCurrentUrl))),
      CommandMapping(kGet, "session/:sessionId/title",
                     WrapToCommand(base::Bind(&ExecuteGetTitle))),
      CommandMapping(kGet, "session/:sessionId/source",
                     WrapToCommand(base::Bind(&ExecuteGetPageSource))),
      CommandMapping(kGet, "session/:sessionId/screenshot",
                     WrapToCommand(base::Bind(&ExecuteScreenshot))),
      CommandMapping(kPost, "session/:sessionId/visible",
                     base::Bind(&UnimplementedCommand)),
      CommandMapping(kGet, "session/:sessionId/visible",
                     base::Bind(&UnimplementedCommand)),
      CommandMapping(kPost, "session/:sessionId/element",
                     WrapToCommand(base::Bind(&ExecuteFindElement, 50))),
      CommandMapping(kPost, "session/:sessionId/elements",
                     WrapToCommand(base::Bind(&ExecuteFindElements, 50))),
      CommandMapping(kPost, "session/:sessionId/element/active",
                     WrapToCommand(base::Bind(&ExecuteGetActiveElement))),
      CommandMapping(kPost, "session/:sessionId/element/:id/element",
                     WrapToCommand(base::Bind(&ExecuteFindChildElement, 50))),
      CommandMapping(kPost, "session/:sessionId/element/:id/elements",
                     WrapToCommand(base::Bind(&ExecuteFindChildElements, 50))),
      CommandMapping(kPost, "session/:sessionId/element/:id/click",
                     WrapToCommand(base::Bind(&ExecuteClickElement))),
      CommandMapping(kPost, "session/:sessionId/element/:id/clear",
                     WrapToCommand(base::Bind(&ExecuteClearElement))),
      CommandMapping(kPost, "session/:sessionId/element/:id/submit",
                     WrapToCommand(base::Bind(&ExecuteSubmitElement))),
      CommandMapping(kGet, "session/:sessionId/element/:id/text",
                     WrapToCommand(base::Bind(&ExecuteGetElementText))),
      CommandMapping(kPost, "session/:sessionId/element/:id/value",
                     WrapToCommand(base::Bind(&ExecuteSendKeysToElement))),
      CommandMapping(kPost, "session/:sessionId/file",
                     WrapToCommand(base::Bind(&ExecuteUploadFile))),
      CommandMapping(kGet, "session/:sessionId/element/:id/value",
                     WrapToCommand(base::Bind(&ExecuteGetElementValue))),
      CommandMapping(kGet, "session/:sessionId/element/:id/name",
                     WrapToCommand(base::Bind(&ExecuteGetElementTagName))),
      CommandMapping(kGet, "session/:sessionId/element/:id/selected",
                     WrapToCommand(base::Bind(&ExecuteIsElementSelected))),
      CommandMapping(kGet, "session/:sessionId/element/:id/enabled",
                     WrapToCommand(base::Bind(&ExecuteIsElementEnabled))),
      CommandMapping(kGet, "session/:sessionId/element/:id/displayed",
                     WrapToCommand(base::Bind(&ExecuteIsElementDisplayed))),
      CommandMapping(kPost, "session/:sessionId/element/:id/hover",
                     WrapToCommand(base::Bind(&ExecuteHoverOverElement))),
      CommandMapping(kGet, "session/:sessionId/element/:id/location",
                     WrapToCommand(base::Bind(&ExecuteGetElementLocation))),
      CommandMapping(kGet, "session/:sessionId/element/:id/location_in_view",
                     WrapToCommand(
                         base::Bind(
                             &ExecuteGetElementLocationOnceScrolledIntoView))),
      CommandMapping(kGet, "session/:sessionId/element/:id/size",
                     WrapToCommand(base::Bind(&ExecuteGetElementSize))),
      CommandMapping(kGet, "session/:sessionId/element/:id/attribute/:name",
                     WrapToCommand(base::Bind(&ExecuteGetElementAttribute))),
      CommandMapping(kGet, "session/:sessionId/element/:id/equals/:other",
                     WrapToCommand(base::Bind(&ExecuteElementEquals))),
      CommandMapping(kGet, "session/:sessionId/cookie",
                     WrapToCommand(base::Bind(&ExecuteGetCookies))),
      CommandMapping(kPost, "session/:sessionId/cookie",
                     WrapToCommand(base::Bind(&ExecuteAddCookie))),
      CommandMapping(kDelete, "session/:sessionId/cookie",
                     WrapToCommand(base::Bind(&ExecuteDeleteAllCookies))),
      CommandMapping(kDelete, "session/:sessionId/cookie/:name",
                     WrapToCommand(base::Bind(&ExecuteDeleteCookie))),
      CommandMapping(kPost, "session/:sessionId/frame",
                     WrapToCommand(base::Bind(&ExecuteSwitchToFrame))),
      CommandMapping(kPost, "session/:sessionId/window",
                     WrapToCommand(base::Bind(&ExecuteSwitchToWindow))),
      CommandMapping(kGet, "session/:sessionId/window/:windowHandle/size",
                     WrapToCommand(base::Bind(&ExecuteGetWindowSize))),
      CommandMapping(kGet, "session/:sessionId/window/:windowHandle/position",
                     WrapToCommand(base::Bind(&ExecuteGetWindowPosition))),
      CommandMapping(kPost, "session/:sessionId/window/:windowHandle/size",
                     WrapToCommand(base::Bind(&ExecuteSetWindowSize))),
      CommandMapping(kPost, "session/:sessionId/window/:windowHandle/position",
                     WrapToCommand(base::Bind(&ExecuteSetWindowPosition))),
      CommandMapping(kPost, "session/:sessionId/window/:windowHandle/maximize",
                     WrapToCommand(base::Bind(&ExecuteMaximizeWindow))),
      CommandMapping(kDelete, "session/:sessionId/window",
                     WrapToCommand(base::Bind(&ExecuteClose, &session_map_))),
      CommandMapping(kPost, "session/:sessionId/element/:id/drag",
                     base::Bind(&UnimplementedCommand)),
      CommandMapping(kGet, "session/:sessionId/element/:id/css/:propertyName",
                     WrapToCommand(
                         base::Bind(&ExecuteGetElementValueOfCSSProperty))),
      CommandMapping(kPost, "session/:sessionId/timeouts/implicit_wait",
                     WrapToCommand(base::Bind(&ExecuteImplicitlyWait))),
      CommandMapping(kPost, "session/:sessionId/timeouts/async_script",
                     WrapToCommand(base::Bind(&ExecuteSetScriptTimeout))),
      CommandMapping(kPost, "session/:sessionId/timeouts",
                     WrapToCommand(base::Bind(&ExecuteSetTimeout))),
      CommandMapping(kPost, "session/:sessionId/execute_sql",
                     base::Bind(&UnimplementedCommand)),
      CommandMapping(kGet, "session/:sessionId/location",
                     WrapToCommand(base::Bind(&ExecuteGetLocation))),
      CommandMapping(kPost, "session/:sessionId/location",
                     WrapToCommand(base::Bind(&ExecuteSetLocation))),
      CommandMapping(kGet, "session/:sessionId/application_cache/status",
                     base::Bind(&ExecuteGetStatus)),
      CommandMapping(kGet, "session/:sessionId/browser_connection",
                     base::Bind(&UnimplementedCommand)),
      CommandMapping(kPost, "session/:sessionId/browser_connection",
                     base::Bind(&UnimplementedCommand)),
      CommandMapping(kGet, "session/:sessionId/local_storage/key/:key",
                     WrapToCommand(
                         base::Bind(&ExecuteGetStorageItem, kLocalStorage))),
      CommandMapping(kDelete, "session/:sessionId/local_storage/key/:key",
                     WrapToCommand(
                         base::Bind(&ExecuteRemoveStorageItem, kLocalStorage))),
      CommandMapping(kGet, "session/:sessionId/local_storage",
                     WrapToCommand(
                         base::Bind(&ExecuteGetStorageKeys, kLocalStorage))),
      CommandMapping(kPost, "session/:sessionId/local_storage",
                     WrapToCommand(
                         base::Bind(&ExecuteSetStorageItem, kLocalStorage))),
      CommandMapping(kDelete, "session/:sessionId/local_storage",
                     WrapToCommand(
                         base::Bind(&ExecuteClearStorage, kLocalStorage))),
      CommandMapping(kGet, "session/:sessionId/local_storage/size",
                     WrapToCommand(
                         base::Bind(&ExecuteGetStorageSize, kLocalStorage))),
      CommandMapping(kGet, "session/:sessionId/session_storage/key/:key",
                     WrapToCommand(
                         base::Bind(&ExecuteGetStorageItem, kSessionStorage))),
      CommandMapping(kDelete, "session/:sessionId/session_storage/key/:key",
                     WrapToCommand(
                         base::Bind(
                             &ExecuteRemoveStorageItem, kSessionStorage))),
      CommandMapping(kGet, "session/:sessionId/session_storage",
                     WrapToCommand(
                         base::Bind(&ExecuteGetStorageKeys, kSessionStorage))),
      CommandMapping(kPost, "session/:sessionId/session_storage",
                     WrapToCommand(
                         base::Bind(&ExecuteSetStorageItem, kSessionStorage))),
      CommandMapping(kDelete, "session/:sessionId/session_storage",
                     WrapToCommand(
                         base::Bind(&ExecuteClearStorage, kSessionStorage))),
      CommandMapping(kGet, "session/:sessionId/session_storage/size",
                     WrapToCommand(
                         base::Bind(&ExecuteGetStorageSize, kSessionStorage))),
      CommandMapping(kGet, "session/:sessionId/orientation",
                     base::Bind(&UnimplementedCommand)),
      CommandMapping(kPost, "session/:sessionId/orientation",
                     base::Bind(&UnimplementedCommand)),
      CommandMapping(kPost, "session/:sessionId/click",
                     WrapToCommand(base::Bind(&ExecuteMouseClick))),
      CommandMapping(kPost, "session/:sessionId/doubleclick",
                     WrapToCommand(base::Bind(&ExecuteMouseDoubleClick))),
      CommandMapping(kPost, "session/:sessionId/buttondown",
                     WrapToCommand(base::Bind(&ExecuteMouseButtonDown))),
      CommandMapping(kPost, "session/:sessionId/buttonup",
                     WrapToCommand(base::Bind(&ExecuteMouseButtonUp))),
      CommandMapping(kPost, "session/:sessionId/moveto",
                     WrapToCommand(base::Bind(&ExecuteMouseMoveTo))),
      CommandMapping(kPost, "session/:sessionId/keys",
                     WrapToCommand(
                         base::Bind(&ExecuteSendKeysToActiveElement))),
      CommandMapping(kGet, "session/:sessionId/ime/available_engines",
                     base::Bind(&UnimplementedCommand)),
      CommandMapping(kGet, "session/:sessionId/ime/active_engine",
                     base::Bind(&UnimplementedCommand)),
      CommandMapping(kGet, "session/:sessionId/ime/activated",
                     base::Bind(&UnimplementedCommand)),
      CommandMapping(kPost, "session/:sessionId/ime/deactivate",
                     base::Bind(&UnimplementedCommand)),
      CommandMapping(kPost, "session/:sessionId/ime/activate",
                     base::Bind(&UnimplementedCommand)),
      CommandMapping(kPost, "session/:sessionId/touch/click",
                     WrapToCommand(base::Bind(&ExecuteTouchSingleTap))),
      CommandMapping(kPost, "session/:sessionId/touch/down",
                     base::Bind(&UnimplementedCommand)),
      CommandMapping(kPost, "session/:sessionId/touch/up",
                     base::Bind(&UnimplementedCommand)),
      CommandMapping(kPost, "session/:sessionId/touch/move",
                     base::Bind(&UnimplementedCommand)),
      CommandMapping(kPost, "session/:sessionId/touch/scroll",
                     base::Bind(&UnimplementedCommand)),
      CommandMapping(kPost, "session/:sessionId/touch/doubleclick",
                     base::Bind(&UnimplementedCommand)),
      CommandMapping(kPost, "session/:sessionId/touch/longclick",
                     base::Bind(&UnimplementedCommand)),
      CommandMapping(kPost, "session/:sessionId/touch/flick",
                     base::Bind(&UnimplementedCommand)),
      CommandMapping(kPost, "session/:sessionId/log",
                     WrapToCommand(base::Bind(&ExecuteGetLog))),
      CommandMapping(kGet, "session/:sessionId/log/types",
                     WrapToCommand(base::Bind(&ExecuteGetAvailableLogTypes))),
      CommandMapping(kPost, "logs", base::Bind(&UnimplementedCommand)),
      CommandMapping(kGet, "status", base::Bind(&ExecuteGetStatus)),

      // Custom Chrome commands:
      // Allow quit all to be called with GET or POST.
      CommandMapping(kGet, kShutdownPath,
                     base::Bind(&ExecuteQuitAll,
                                base::Bind(&ExecuteQuit, true, &session_map_),
                                &session_map_)),
      CommandMapping(kPost, kShutdownPath,
                     base::Bind(&ExecuteQuitAll,
                                base::Bind(&ExecuteQuit, true, &session_map_),
                                &session_map_)),
      CommandMapping(kGet, "session/:sessionId/is_loading",
                     WrapToCommand(base::Bind(&ExecuteIsLoading))),
  };
  this->command_map_.reset(
      new CommandMap(commands, commands + arraysize(commands)));
}

HttpHandler::~HttpHandler() {}

void HttpHandler::Handle(const net::HttpServerRequestInfo& request,
                         HttpResponse* response) {
  log_->AddEntry(
      Log::kLog,
      base::StringPrintf("received WebDriver request: %s %s %s",
                         request.method.c_str(),
                         request.path.c_str(),
                         request.data.c_str()));

  HandleInternal(request, response);

  log_->AddEntry(
      Log::kLog,
      base::StringPrintf("sending WebDriver response: %d %s",
                         response->status(),
                         response->body().c_str()));
}

bool HttpHandler::ShouldShutdown(const net::HttpServerRequestInfo& request) {
  return request.path == url_base_ + kShutdownPath;
}

Command HttpHandler::WrapToCommand(
    const SessionCommand& session_command) {
  return base::Bind(&ExecuteSessionCommand, &session_map_, session_command);
}

Command HttpHandler::WrapToCommand(
    const WindowCommand& window_command) {
  return WrapToCommand(
      base::Bind(&ExecuteWindowCommand, window_command));
}

Command HttpHandler::WrapToCommand(
    const ElementCommand& element_command) {
  return WrapToCommand(
      base::Bind(&ExecuteElementCommand, element_command));
}

void HttpHandler::HandleInternal(const net::HttpServerRequestInfo& request,
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

bool HttpHandler::HandleWebDriverCommand(
    const net::HttpServerRequestInfo& request,
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

  if (request.data.length()) {
    base::DictionaryValue* body_params;
    scoped_ptr<base::Value> parsed_body(base::JSONReader::Read(request.data));
    if (!parsed_body || !parsed_body->GetAsDictionary(&body_params)) {
      *response = HttpResponse(HttpResponse::kBadRequest);
      response->set_body("missing command parameters");
      return true;
    }
    params.MergeDictionary(body_params);
  }

  scoped_ptr<base::Value> value;
  std::string out_session_id;
  Status status = iter->command.Run(
      params, session_id, &value, &out_session_id);

  if (status.code() == kUnknownCommand) {
    *response = HttpResponse(HttpResponse::kNotImplemented);
    response->set_body("unimplemented command: " + trimmed_path);
    return true;
  }

  if (iter->path_pattern == internal::kNewSessionPathPattern && status.IsOk()) {
    // Creating a session involves a HTTP request to /session, which is
    // supposed to redirect to /session/:sessionId, which returns the
    // session info.
    *response = HttpResponse(HttpResponse::kSeeOther);
    response->AddHeader("Location", url_base_ + "session/" + out_session_id);
    return true;
  } else if (status.IsError()) {
    status.AddDetails(base::StringPrintf(
        "Driver info: chromedriver=%s,platform=%s %s %s",
        kChromeDriverVersion,
        base::SysInfo::OperatingSystemName().c_str(),
        base::SysInfo::OperatingSystemVersion().c_str(),
        base::SysInfo::OperatingSystemArchitecture().c_str()));
    scoped_ptr<base::DictionaryValue> error(new base::DictionaryValue());
    error->SetString("message", status.message());
    value.reset(error.release());
  }
  if (!value)
    value.reset(base::Value::CreateNullValue());

  base::DictionaryValue body_params;
  body_params.SetInteger("status", status.code());
  body_params.Set("value", value.release());
  body_params.SetString("sessionId", out_session_id);
  std::string body;
  base::JSONWriter::WriteWithOptions(
      &body_params, base::JSONWriter::OPTIONS_OMIT_DOUBLE_TYPE_PRESERVATION,
      &body);
  *response = HttpResponse(HttpResponse::kOk);
  response->SetMimeType("application/json; charset=utf-8");
  response->set_body(body);
  return true;
}

namespace internal {

const char kNewSessionPathPattern[] = "session";

bool MatchesMethod(HttpMethod command_method, const std::string& method) {
  std::string lower_method = StringToLowerASCII(method);
  switch (command_method) {
  case kGet:
    return lower_method == "get";
  case kPost:
    return lower_method == "post" || lower_method == "put";
  case kDelete:
    return lower_method == "delete";
  }
  return false;
}

bool MatchesCommand(const std::string& method,
                    const std::string& path,
                    const CommandMapping& command,
                    std::string* session_id,
                    base::DictionaryValue* out_params) {
  if (!MatchesMethod(command.method, method))
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

}  // namespace internal
