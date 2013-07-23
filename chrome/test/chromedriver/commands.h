// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_COMMANDS_H_
#define CHROME_TEST_CHROMEDRIVER_COMMANDS_H_

#include <string>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/test/chromedriver/command.h"
#include "chrome/test/chromedriver/net/sync_websocket_factory.h"
#include "chrome/test/chromedriver/session_map.h"

namespace base {
class DictionaryValue;
class Value;
}

class DeviceManager;
class Log;
class Status;
class URLRequestContextGetter;

// Gets status/info about ChromeDriver.
Status ExecuteGetStatus(
    const base::DictionaryValue& params,
    const std::string& session_id,
    scoped_ptr<base::Value>* out_value,
    std::string* out_session_id);

struct NewSessionParams {
  NewSessionParams(Log* log,
                   SessionMap* session_map,
                   scoped_refptr<URLRequestContextGetter> context_getter,
                   const SyncWebSocketFactory& socket_factory,
                   DeviceManager* device_manager);
  ~NewSessionParams();

  Log* log;
  SessionMap* session_map;
  scoped_refptr<URLRequestContextGetter> context_getter;
  SyncWebSocketFactory socket_factory;
  DeviceManager* device_manager;
};

// Creates a new session.
Status ExecuteNewSession(
    const NewSessionParams& bound_params,
    const base::DictionaryValue& params,
    const std::string& session_id,
    scoped_ptr<base::Value>* out_value,
    std::string* out_session_id);

// Quits a particular session.
Status ExecuteQuit(
    bool allow_detach,
    SessionMap* session_map,
    const base::DictionaryValue& params,
    const std::string& session_id,
    scoped_ptr<base::Value>* out_value,
    std::string* out_session_id);

// Quits all sessions.
Status ExecuteQuitAll(
    Command quit_command,
    SessionMap* session_map,
    const base::DictionaryValue& params,
    const std::string& session_id,
    scoped_ptr<base::Value>* out_value,
    std::string* out_session_id);

#endif  // CHROME_TEST_CHROMEDRIVER_COMMANDS_H_
