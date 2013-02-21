// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/commands.h"

#include "base/callback.h"
#include "base/environment.h"
#include "base/file_util.h"
#include "base/string_split.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome.h"
#include "chrome/test/chromedriver/chrome_android_impl.h"
#include "chrome/test/chromedriver/chrome_desktop_impl.h"
#include "chrome/test/chromedriver/net/url_request_context_getter.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/session_map.h"
#include "chrome/test/chromedriver/status.h"
#include "chrome/test/chromedriver/util.h"
#include "chrome/test/chromedriver/version.h"
#include "chrome/test/chromedriver/web_view.h"

namespace {

Status CheckChromeVersion(WebView* web_view, std::string* chrome_version) {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  scoped_ptr<base::Value> chrome_version_value;
  std::string temp_chrome_version;
  Status status = web_view->EvaluateScript(
      "",
      "navigator.appVersion.match(/Chrome\\/.* /)[0].split('/')[1].trim()",
      &chrome_version_value);
  if (status.IsError() ||
      !chrome_version_value->GetAsString(&temp_chrome_version))
    return Status(kUnknownError, "unable to detect Chrome version");

  int build_no;
  std::vector<std::string> chrome_version_parts;
  base::SplitString(temp_chrome_version, '.', &chrome_version_parts);
  if (chrome_version_parts.size() != 4 ||
      !base::StringToInt(chrome_version_parts[2], &build_no)) {
    return Status(kUnknownError, "unrecognized Chrome version: " +
        temp_chrome_version);
  }
  // Allow the version check to be skipped for testing/development purposes.
  if (!env->HasVar("IGNORE_CHROME_VERSION")) {
    if (build_no < kMinimumSupportedChromeBuildNo) {
      return Status(kUnknownError, "Chrome version must be >= " +
          GetMinimumSupportedChromeVersion());
    }
  }
  *chrome_version = temp_chrome_version;
  return Status(kOk);
}

}  // namespace

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

  scoped_ptr<Chrome> chrome;
  Status status(kOk);
  int port = 33081;
  std::string landing_url("data:text/html;charset=utf-8,");
  std::string android_package;

  if (params.GetString("desiredCapabilities.chromeOptions.android_package",
                       &android_package)) {
    scoped_ptr<ChromeAndroidImpl> chrome_android(new ChromeAndroidImpl(
        context_getter, port, socket_factory));
    status = chrome_android->Launch(android_package, landing_url);
    chrome.reset(chrome_android.release());
  } else {
    base::FilePath::StringType path_str;
    base::FilePath chrome_exe;
    if (params.GetString("desiredCapabilities.chromeOptions.binary",
                         &path_str)) {
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
    if (params.Get("desiredCapabilities.chromeOptions.args", &args) &&
        !args->GetAsList(&args_list)) {
        return Status(kUnknownError,
                      "command line arguments for chrome must be a list");
    }

    scoped_ptr<ChromeDesktopImpl> chrome_desktop(new ChromeDesktopImpl(
        context_getter, port, socket_factory));
    status = chrome_desktop->Launch(chrome_exe, args_list, landing_url);
    chrome.reset(chrome_desktop.release());
  }
  if (status.IsError())
    return Status(kSessionNotCreatedException, status.message());

  std::list<WebView*> web_views;
  status = chrome->GetWebViews(&web_views);
  if (status.IsError() || web_views.empty()) {
    chrome->Quit();
    return status.IsError() ? status :
        Status(kUnknownError, "unable to discover open window in chrome");
  }
  WebView* default_web_view = web_views.front();

  std::string chrome_version;
  status = CheckChromeVersion(default_web_view, &chrome_version);
  if (status.IsError()) {
    chrome->Quit();
    return status;
  }

  std::string new_id = session_id;
  if (new_id.empty())
    new_id = GenerateId();
  scoped_ptr<Session> session(new Session(new_id, chrome.Pass()));
  session->window = default_web_view->GetId();
  scoped_refptr<SessionAccessor> accessor(
      new SessionAccessorImpl(session.Pass()));
  session_map->Set(new_id, accessor);

  base::DictionaryValue* returned_value = new base::DictionaryValue();
  returned_value->SetString("browserName", "chrome");
  returned_value->SetString("version", chrome_version);
  returned_value->SetString("driverVersion", kChromeDriverVersion);
  out_value->reset(returned_value);
  *out_session_id = new_id;
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
