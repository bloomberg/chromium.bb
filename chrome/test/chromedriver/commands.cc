// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/commands.h"

#include <list>

#include "base/stringprintf.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "chrome/test/chromedriver/capabilities.h"
#include "chrome/test/chromedriver/chrome/chrome.h"
#include "chrome/test/chromedriver/chrome/chrome_android_impl.h"
#include "chrome/test/chromedriver/chrome/chrome_desktop_impl.h"
#include "chrome/test/chromedriver/chrome/devtools_event_logger.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/version.h"
#include "chrome/test/chromedriver/chrome/web_view.h"
#include "chrome/test/chromedriver/chrome_launcher.h"
#include "chrome/test/chromedriver/logging.h"
#include "chrome/test/chromedriver/net/net_util.h"
#include "chrome/test/chromedriver/net/url_request_context_getter.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/session_map.h"
#include "chrome/test/chromedriver/util.h"

Status ExecuteGetStatus(
    const base::DictionaryValue& params,
    const std::string& session_id,
    scoped_ptr<base::Value>* out_value,
    std::string* out_session_id) {
  base::DictionaryValue build;
  build.SetString("version", "alpha");

  base::DictionaryValue os;
  os.SetString("name", base::SysInfo::OperatingSystemName());
  os.SetString("version", base::SysInfo::OperatingSystemVersion());
  os.SetString("arch", base::SysInfo::OperatingSystemArchitecture());

  base::DictionaryValue info;
  info.Set("build", build.DeepCopy());
  info.Set("os", os.DeepCopy());
  out_value->reset(info.DeepCopy());
  return Status(kOk);
}

Status ExecuteNewSession(
    SessionMap* session_map,
    scoped_refptr<URLRequestContextGetter> context_getter,
    const SyncWebSocketFactory& socket_factory,
    const base::DictionaryValue& params,
    const std::string& session_id,
    scoped_ptr<base::Value>* out_value,
    std::string* out_session_id) {
  int port;
  if (!FindOpenPort(&port))
    return Status(kUnknownError, "failed to find an open port for Chrome");

  const base::DictionaryValue* desired_caps;
  if (!params.GetDictionary("desiredCapabilities", &desired_caps))
    return Status(kUnknownError, "cannot find dict 'desiredCapabilities'");

  Capabilities capabilities;
  Status status = capabilities.Parse(*desired_caps);
  if (status.IsError())
    return status;

  // Create DevToolsEventLoggers, fail if log levels are invalid.
  ScopedVector<DevToolsEventLogger> devtools_event_loggers;
  status = CreateLoggers(capabilities, &devtools_event_loggers);
  if (status.IsError())
    return status;

  scoped_ptr<Chrome> chrome;
  std::list<DevToolsEventLogger*> devtools_event_logger_list(
      devtools_event_loggers.begin(), devtools_event_loggers.end());
  status = LaunchChrome(context_getter, port, socket_factory,
                        capabilities, devtools_event_logger_list, &chrome);
  if (status.IsError())
    return status;

  std::list<std::string> web_view_ids;
  status = chrome->GetWebViewIds(&web_view_ids);
  if (status.IsError() || web_view_ids.empty()) {
    chrome->Quit();
    return status.IsError() ? status :
        Status(kUnknownError, "unable to discover open window in chrome");
  }

  std::string new_id = session_id;
  if (new_id.empty())
    new_id = GenerateId();
  scoped_ptr<Session> session(new Session(new_id, chrome.Pass()));
  session->devtools_event_loggers.swap(devtools_event_loggers);
  if (!session->thread.Start()) {
    chrome->Quit();
    return Status(kUnknownError,
                  "failed to start a thread for the new session");
  }
  session->window = web_view_ids.front();
  out_value->reset(session->capabilities->DeepCopy());
  *out_session_id = new_id;

  scoped_refptr<SessionAccessor> accessor(
      new SessionAccessorImpl(session.Pass()));
  session_map->Set(new_id, accessor);

  return Status(kOk);
}

Status ExecuteQuitAll(
    Command quit_command,
    SessionMap* session_map,
    const base::DictionaryValue& params,
    const std::string& session_id,
    scoped_ptr<base::Value>* out_value,
    std::string* out_session_id) {
  std::vector<std::string> session_ids;
  session_map->GetKeys(&session_ids);
  for (size_t i = 0; i < session_ids.size(); ++i) {
    scoped_ptr<base::Value> unused_value;
    std::string unused_session_id;
    quit_command.Run(params, session_ids[i], &unused_value, &unused_session_id);
  }
  return Status(kOk);
}
