// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/commands.h"

#include <list>

#include "base/logging.h"  // For CHECK macros.
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "chrome/test/chromedriver/capabilities.h"
#include "chrome/test/chromedriver/chrome/chrome.h"
#include "chrome/test/chromedriver/chrome/chrome_android_impl.h"
#include "chrome/test/chromedriver/chrome/chrome_desktop_impl.h"
#include "chrome/test/chromedriver/chrome/device_manager.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
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

NewSessionParams::NewSessionParams(
    Log* log,
    SessionMap* session_map,
    scoped_refptr<URLRequestContextGetter> context_getter,
    const SyncWebSocketFactory& socket_factory,
    DeviceManager* device_manager)
    : log(log),
      session_map(session_map),
      context_getter(context_getter),
      socket_factory(socket_factory),
      device_manager(device_manager) {}

NewSessionParams::~NewSessionParams() {}

Status ExecuteNewSession(
    const NewSessionParams& bound_params,
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

  // Create Log's and DevToolsEventListener's for ones that are DevTools-based.
  // Session will own the Log's, Chrome will own the listeners.
  ScopedVector<WebDriverLog> devtools_logs;
  ScopedVector<DevToolsEventListener> devtools_event_listeners;
  status = CreateLogs(capabilities, &devtools_logs, &devtools_event_listeners);
  if (status.IsError())
    return status;

  scoped_ptr<Chrome> chrome;
  status = LaunchChrome(bound_params.context_getter.get(),
                        port,
                        bound_params.socket_factory,
                        bound_params.log,
                        bound_params.device_manager,
                        capabilities,
                        devtools_event_listeners,
                        &chrome);
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
  session->devtools_logs.swap(devtools_logs);
  if (!session->thread.Start()) {
    chrome->Quit();
    return Status(kUnknownError,
                  "failed to start a thread for the new session");
  }
  session->window = web_view_ids.front();
  session->detach = capabilities.detach;
  out_value->reset(session->capabilities->DeepCopy());
  *out_session_id = new_id;

  scoped_refptr<SessionAccessor> accessor(
      new SessionAccessorImpl(session.Pass()));
  bound_params.session_map->Set(new_id, accessor);

  return Status(kOk);
}

Status ExecuteQuit(
    bool allow_detach,
    SessionMap* session_map,
    const base::DictionaryValue& params,
    const std::string& session_id,
    scoped_ptr<base::Value>* out_value,
    std::string* out_session_id) {
  *out_session_id = session_id;
  scoped_refptr<SessionAccessor> session_accessor;
  if (!session_map->Get(session_id, &session_accessor))
    return Status(kOk);
  scoped_ptr<base::AutoLock> session_lock;
  Session* session = session_accessor->Access(&session_lock);
  if (!session)
    return Status(kOk);
  CHECK(session_map->Remove(session->id));
  if (allow_detach && session->detach) {
    session_accessor->DeleteSession();
    return Status(kOk);
  } else {
    Status status = session->chrome->Quit();
    session_accessor->DeleteSession();
    return status;
  }
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
