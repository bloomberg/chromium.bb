// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/commands.h"

#include <list>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/logging.h"  // For CHECK macros.
#include "base/memory/linked_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/threading/thread_local.h"
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
#include "chrome/test/chromedriver/session_thread_map.h"
#include "chrome/test/chromedriver/util.h"

void ExecuteGetStatus(
    const base::DictionaryValue& params,
    const std::string& session_id,
    const CommandCallback& callback) {
  base::DictionaryValue build;
  build.SetString("version", "alpha");

  base::DictionaryValue os;
  os.SetString("name", base::SysInfo::OperatingSystemName());
  os.SetString("version", base::SysInfo::OperatingSystemVersion());
  os.SetString("arch", base::SysInfo::OperatingSystemArchitecture());

  base::DictionaryValue info;
  info.Set("build", build.DeepCopy());
  info.Set("os", os.DeepCopy());
  callback.Run(
      Status(kOk), scoped_ptr<base::Value>(info.DeepCopy()), std::string());
}

NewSessionParams::NewSessionParams(
    Log* log,
    SessionThreadMap* session_thread_map,
    scoped_refptr<URLRequestContextGetter> context_getter,
    const SyncWebSocketFactory& socket_factory,
    DeviceManager* device_manager)
    : log(log),
      session_thread_map(session_thread_map),
      context_getter(context_getter),
      socket_factory(socket_factory),
      device_manager(device_manager) {}

NewSessionParams::~NewSessionParams() {}

namespace {

base::LazyInstance<base::ThreadLocalPointer<Session> >
    lazy_tls_session = LAZY_INSTANCE_INITIALIZER;

Status CreateSessionOnSessionThreadHelper(
    const NewSessionParams& bound_params,
    const base::DictionaryValue& params,
    const std::string& session_id,
    scoped_ptr<base::Value>* out_value) {
  int port;
  if (!FindOpenPort(&port))
    return Status(kUnknownError, "failed to find an open port for Chrome");

  const base::DictionaryValue* desired_caps;
  if (!params.GetDictionary("desiredCapabilities", &desired_caps))
    return Status(kUnknownError, "cannot find dict 'desiredCapabilities'");

  Capabilities capabilities;
  Status status = capabilities.Parse(*desired_caps, bound_params.log);
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

  scoped_ptr<Session> session(new Session(session_id, chrome.Pass()));
  session->devtools_logs.swap(devtools_logs);
  session->window = web_view_ids.front();
  session->detach = capabilities.detach;
  out_value->reset(session->capabilities->DeepCopy());
  lazy_tls_session.Pointer()->Set(session.release());
  return Status(kOk);
}

void CreateSessionOnSessionThread(
    const scoped_refptr<base::SingleThreadTaskRunner>& cmd_task_runner,
    const NewSessionParams& bound_params,
    scoped_ptr<base::DictionaryValue> params,
    const std::string& session_id,
    const CommandCallback& callback_on_cmd) {
  scoped_ptr<base::Value> value;
  Status status = CreateSessionOnSessionThreadHelper(
      bound_params, *params, session_id, &value);
  cmd_task_runner->PostTask(
      FROM_HERE,
      base::Bind(callback_on_cmd, status, base::Passed(&value), session_id));
}

}  // namespace

void ExecuteNewSession(
    const NewSessionParams& bound_params,
    const base::DictionaryValue& params,
    const std::string& session_id,
    const CommandCallback& callback) {
  std::string new_id = session_id;
  if (new_id.empty())
    new_id = GenerateId();
  scoped_ptr<base::Thread> thread(new base::Thread(new_id.c_str()));
  if (!thread->Start()) {
    callback.Run(
        Status(kUnknownError, "failed to start a thread for the new session"),
        scoped_ptr<base::Value>(),
        std::string());
    return;
  }

  thread->message_loop()
      ->PostTask(FROM_HERE,
                 base::Bind(&CreateSessionOnSessionThread,
                            base::MessageLoopProxy::current(),
                            bound_params,
                            base::Passed(make_scoped_ptr(params.DeepCopy())),
                            new_id,
                            callback));
  bound_params.session_thread_map
      ->insert(std::make_pair(new_id, make_linked_ptr(thread.release())));
}

namespace {

void OnSessionQuit(const base::WeakPtr<size_t>& quit_remaining_count,
                   const base::Closure& all_quit_func,
                   const Status& status,
                   scoped_ptr<base::Value> value,
                   const std::string& session_id) {
  // |quit_remaining_count| may no longer be valid if a timeout occurred.
  if (!quit_remaining_count)
    return;

  (*quit_remaining_count)--;
  if (!*quit_remaining_count)
    all_quit_func.Run();
}

}  // namespace

void ExecuteQuitAll(
    const Command& quit_command,
    SessionThreadMap* session_thread_map,
    const base::DictionaryValue& params,
    const std::string& session_id,
    const CommandCallback& callback) {
  size_t quit_remaining_count = session_thread_map->size();
  base::WeakPtrFactory<size_t> weak_ptr_factory(&quit_remaining_count);
  if (!quit_remaining_count) {
    callback.Run(Status(kOk), scoped_ptr<base::Value>(), session_id);
    return;
  }
  base::RunLoop run_loop;
  for (SessionThreadMap::const_iterator iter = session_thread_map->begin();
       iter != session_thread_map->end();
       ++iter) {
    quit_command.Run(params,
                     iter->first,
                     base::Bind(&OnSessionQuit,
                                weak_ptr_factory.GetWeakPtr(),
                                run_loop.QuitClosure()));
  }
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::TimeDelta::FromSeconds(10));
  // Uses a nested run loop to block this thread until all the quit
  // commands have executed, or the timeout expires.
  base::MessageLoop::current()->SetNestableTasksAllowed(true);
  run_loop.Run();
  callback.Run(Status(kOk), scoped_ptr<base::Value>(), session_id);
}

namespace {

void TerminateSessionThreadOnCommandThread(SessionThreadMap* session_thread_map,
                                           const std::string& session_id) {
  session_thread_map->erase(session_id);
}

void ExecuteSessionCommandOnSessionThread(
    const SessionCommand& command,
    bool return_ok_without_session,
    scoped_ptr<base::DictionaryValue> params,
    scoped_refptr<base::SingleThreadTaskRunner> cmd_task_runner,
    const CommandCallback& callback_on_cmd,
    const base::Closure& terminate_on_cmd) {
  Session* session = lazy_tls_session.Pointer()->Get();
  if (!session) {
    cmd_task_runner->PostTask(
        FROM_HERE,
        base::Bind(callback_on_cmd,
                   Status(return_ok_without_session ? kOk : kNoSuchSession),
                   base::Passed(scoped_ptr<base::Value>()),
                   std::string()));
    return;
  }

  scoped_ptr<base::Value> value;
  Status status = command.Run(session, *params, &value);
  if (status.IsError() && session->chrome)
    status.AddDetails("Session info: chrome=" + session->chrome->GetVersion());

  cmd_task_runner->PostTask(
      FROM_HERE,
      base::Bind(callback_on_cmd, status, base::Passed(&value), session->id));

  if (session->quit) {
    lazy_tls_session.Pointer()->Set(NULL);
    delete session;
    cmd_task_runner->PostTask(FROM_HERE, terminate_on_cmd);
  }
}

}  // namespace

void ExecuteSessionCommand(
    SessionThreadMap* session_thread_map,
    const SessionCommand& command,
    bool return_ok_without_session,
    const base::DictionaryValue& params,
    const std::string& session_id,
    const CommandCallback& callback) {
  SessionThreadMap::iterator iter = session_thread_map->find(session_id);
  if (iter == session_thread_map->end()) {
    Status status(return_ok_without_session ? kOk : kNoSuchSession);
    callback.Run(status, scoped_ptr<base::Value>(), session_id);
  } else {
    iter->second->message_loop()
        ->PostTask(FROM_HERE,
                   base::Bind(&ExecuteSessionCommandOnSessionThread,
                              command,
                              return_ok_without_session,
                              base::Passed(make_scoped_ptr(params.DeepCopy())),
                              base::MessageLoopProxy::current(),
                              callback,
                              base::Bind(&TerminateSessionThreadOnCommandThread,
                                         session_thread_map,
                                         session_id)));
  }
}

namespace internal {

void CreateSessionOnSessionThreadForTesting(const std::string& id) {
  lazy_tls_session.Pointer()->Set(new Session(id));
}

}  // namespace internal
