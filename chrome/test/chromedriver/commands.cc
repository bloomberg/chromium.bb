// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/commands.h"

#include "base/callback.h"
#include "base/file_util.h"
#include "base/stringprintf.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/chrome.h"
#include "chrome/test/chromedriver/chrome/chrome_android_impl.h"
#include "chrome/test/chromedriver/chrome/chrome_desktop_impl.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/version.h"
#include "chrome/test/chromedriver/chrome/web_view.h"
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

  scoped_ptr<Chrome> chrome;
  Status status(kOk);
  std::string android_package;
  if (desired_caps->GetString("chromeOptions.android_package",
                              &android_package)) {
    scoped_ptr<ChromeAndroidImpl> chrome_android(new ChromeAndroidImpl(
        context_getter, port, socket_factory));
    status = chrome_android->Launch(android_package);
    chrome.reset(chrome_android.release());
  } else {
    base::FilePath::StringType path_str;
    base::FilePath chrome_exe;
    if (desired_caps->GetString("chromeOptions.binary", &path_str)) {
      chrome_exe = base::FilePath(path_str);
      if (!file_util::PathExists(chrome_exe)) {
        std::string message = base::StringPrintf(
            "no chrome binary at %" PRFilePath,
            path_str.c_str());
        return Status(kUnknownError, message);
      }
    }

    const base::Value* args = NULL;
    const base::ListValue* args_list = NULL;
    if (desired_caps->Get("chromeOptions.args", &args) &&
        !args->GetAsList(&args_list)) {
      return Status(kUnknownError,
                    "command line arguments for chrome must be a list");
    }

    const base::Value* prefs = NULL;
    const base::DictionaryValue* prefs_dict = NULL;
    if (desired_caps->Get("chromeOptions.prefs", &prefs) &&
        !prefs->GetAsDictionary(&prefs_dict)) {
      return Status(kUnknownError, "'prefs' must be a dictionary");
    }

    const base::Value* local_state = NULL;
    const base::DictionaryValue* local_state_dict = NULL;
    if (desired_caps->Get("chromeOptions.localState", &local_state) &&
        !prefs->GetAsDictionary(&prefs_dict)) {
      return Status(kUnknownError, "'localState' must be a dictionary");
    }

    const base::Value* extensions = NULL;
    const base::ListValue* extensions_list = NULL;
    if (desired_caps->Get("chromeOptions.extensions", &extensions)
        && !extensions->GetAsList(&extensions_list)) {
      return Status(kUnknownError,
                    "chrome extensions must be a list");
    }

    const base::Value* log_path = NULL;
    std::string chrome_log_path;
    if (desired_caps->Get("chromeOptions.logPath", &log_path) &&
        !log_path->GetAsString(&chrome_log_path)) {
      return Status(kUnknownError,
                    "chrome log path must be a string");
    }

    scoped_ptr<ChromeDesktopImpl> chrome_desktop(new ChromeDesktopImpl(
        context_getter, port, socket_factory));
    status = chrome_desktop->Launch(chrome_exe, args_list, extensions_list,
                                    prefs_dict, local_state_dict,
                                    chrome_log_path);
    chrome.reset(chrome_desktop.release());
  }
  if (status.IsError())
    return Status(kSessionNotCreatedException, status.message());

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
