// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/command_executor_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/stringprintf.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "chrome/test/chromedriver/command_names.h"
#include "chrome/test/chromedriver/commands.h"
#include "chrome/test/chromedriver/element_commands.h"
#include "chrome/test/chromedriver/net/url_request_context_getter.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/session_commands.h"
#include "chrome/test/chromedriver/session_map.h"
#include "chrome/test/chromedriver/status.h"
#include "chrome/test/chromedriver/version.h"
#include "chrome/test/chromedriver/window_commands.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

CommandExecutorImpl::CommandExecutorImpl()
    : io_thread_("ChromeDriver IO") {}

CommandExecutorImpl::~CommandExecutorImpl() {}

void CommandExecutorImpl::Init() {
#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool autorelease_pool;
#endif
  base::Thread::Options options(MessageLoop::TYPE_IO, 0);
  CHECK(io_thread_.StartWithOptions(options));
  context_getter_ = new URLRequestContextGetter(
      io_thread_.message_loop_proxy());
  socket_factory_ = CreateSyncWebSocketFactory(context_getter_);

  // Commands which require an element id.
  typedef std::map<std::string, ElementCommand> ElementCommandMap;
  ElementCommandMap element_command_map;
  element_command_map[CommandNames::kFindChildElement] =
      base::Bind(&ExecuteFindChildElement, 50);
  element_command_map[CommandNames::kFindChildElements] =
      base::Bind(&ExecuteFindChildElements, 50);
  element_command_map[CommandNames::kHoverOverElement] =
      base::Bind(&ExecuteHoverOverElement);
  element_command_map[CommandNames::kClickElement] =
      base::Bind(&ExecuteClickElement);
  element_command_map[CommandNames::kClearElement] =
      base::Bind(&ExecuteClearElement);
  element_command_map[CommandNames::kSendKeysToElement] =
      base::Bind(&ExecuteSendKeysToElement);

  // Commands which require a window.
  typedef std::map<std::string, WindowCommand> WindowCommandMap;
  WindowCommandMap window_command_map;
  // Wrap ElementCommand into WindowCommand.
  for (ElementCommandMap::const_iterator it = element_command_map.begin();
       it != element_command_map.end(); ++it) {
    window_command_map[it->first] =
        base::Bind(&ExecuteElementCommand, it->second);
  }
  window_command_map[CommandNames::kGet] = base::Bind(&ExecuteGet);
  window_command_map[CommandNames::kExecuteScript] =
      base::Bind(&ExecuteExecuteScript);
  window_command_map[CommandNames::kSwitchToFrame] =
      base::Bind(&ExecuteSwitchToFrame);
  window_command_map[CommandNames::kGetTitle] =
      base::Bind(&ExecuteGetTitle);
  window_command_map[CommandNames::kGetPageSource] =
      base::Bind(&ExecuteGetPageSource);
  window_command_map[CommandNames::kFindElement] =
      base::Bind(&ExecuteFindElement, 50);
  window_command_map[CommandNames::kFindElements] =
      base::Bind(&ExecuteFindElements, 50);
  window_command_map[CommandNames::kGetCurrentUrl] =
      base::Bind(&ExecuteGetCurrentUrl);
  window_command_map[CommandNames::kGoBack] =
      base::Bind(&ExecuteGoBack);
  window_command_map[CommandNames::kGoForward] =
      base::Bind(&ExecuteGoForward);
  window_command_map[CommandNames::kRefresh] =
      base::Bind(&ExecuteRefresh);
  window_command_map[CommandNames::kMouseMoveTo] =
      base::Bind(&ExecuteMouseMoveTo);
  window_command_map[CommandNames::kMouseClick] =
      base::Bind(&ExecuteMouseClick);
  window_command_map[CommandNames::kMouseButtonDown] =
      base::Bind(&ExecuteMouseButtonDown);
  window_command_map[CommandNames::kMouseButtonUp] =
      base::Bind(&ExecuteMouseButtonUp);
  window_command_map[CommandNames::kMouseDoubleClick] =
      base::Bind(&ExecuteMouseDoubleClick);

  // Commands which require a session.
  typedef std::map<std::string, SessionCommand> SessionCommandMap;
  SessionCommandMap session_command_map;
  // Wrap WindowCommand into SessionCommand.
  for (WindowCommandMap::const_iterator it = window_command_map.begin();
       it != window_command_map.end(); ++it) {
    session_command_map[it->first] =
        base::Bind(&ExecuteWindowCommand, it->second);
  }
  session_command_map[CommandNames::kQuit] =
      base::Bind(&ExecuteQuit, &session_map_);
  session_command_map[CommandNames::kGetCurrentWindowHandle] =
      base::Bind(&ExecuteGetCurrentWindowHandle);
  session_command_map[CommandNames::kClose] =
      base::Bind(&ExecuteClose);
  session_command_map[CommandNames::kGetWindowHandles] =
      base::Bind(&ExecuteGetWindowHandles);
  session_command_map[CommandNames::kSwitchToWindow] =
      base::Bind(&ExecuteSwitchToWindow);
  session_command_map[CommandNames::kSetTimeout] =
      base::Bind(&ExecuteSetTimeout);
  session_command_map[CommandNames::kGetAlert] =
      base::Bind(&ExecuteGetAlert);
  session_command_map[CommandNames::kGetAlertText] =
      base::Bind(&ExecuteGetAlertText);
  session_command_map[CommandNames::kSetAlertValue] =
      base::Bind(&ExecuteSetAlertValue);
  session_command_map[CommandNames::kAcceptAlert] =
      base::Bind(&ExecuteAcceptAlert);
  session_command_map[CommandNames::kDismissAlert] =
      base::Bind(&ExecuteDismissAlert);

  // Wrap SessionCommand into non-session Command.
  base::Callback<Status(
      const SessionCommand&,
      const base::DictionaryValue&,
      const std::string&,
      scoped_ptr<base::Value>*,
      std::string*)> execute_session_command = base::Bind(
          &ExecuteSessionCommand,
          &session_map_);
  for (SessionCommandMap::const_iterator it = session_command_map.begin();
       it != session_command_map.end(); ++it) {
    command_map_.Set(it->first,
                     base::Bind(execute_session_command, it->second));
  }
  command_map_.Set(CommandNames::kStatus, base::Bind(&ExecuteGetStatus));
  command_map_.Set(
      CommandNames::kNewSession,
      base::Bind(&ExecuteNewSession, &session_map_, context_getter_,
                 socket_factory_));
  command_map_.Set(
      CommandNames::kQuitAll,
      base::Bind(&ExecuteQuitAll,
                 base::Bind(execute_session_command,
                            base::Bind(&ExecuteQuit, &session_map_)),
                 &session_map_));
}

void CommandExecutorImpl::ExecuteCommand(
    const std::string& name,
    const base::DictionaryValue& params,
    const std::string& session_id,
    StatusCode* status_code,
    scoped_ptr<base::Value>* value,
    std::string* out_session_id) {
#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool autorelease_pool;
#endif
  Command cmd;
  Status status(kOk);
  if (command_map_.Get(name, &cmd)) {
    status = cmd.Run(params, session_id, value, out_session_id);
  } else {
    status = Status(kUnknownCommand, name);
    *out_session_id = session_id;
  }
  *status_code = status.code();
  if (status.IsError()) {
    status.AddDetails(base::StringPrintf(
        "Driver info: chromedriver=%s,platform=%s %s %s",
        kChromeDriverVersion,
        base::SysInfo::OperatingSystemName().c_str(),
        base::SysInfo::OperatingSystemVersion().c_str(),
        base::SysInfo::OperatingSystemArchitecture().c_str()));
    scoped_ptr<base::DictionaryValue> error(new base::DictionaryValue());
    error->SetString("message", status.message());
    value->reset(error.release());
  }
  if (!*value)
    value->reset(base::Value::CreateNullValue());
}
