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
#include "chrome/test/chromedriver/session_thread_map.h"

namespace base {
class DictionaryValue;
class Value;
}

class DeviceManager;
class Log;
struct Session;
class Status;
class URLRequestContextGetter;

// Gets status/info about ChromeDriver.
void ExecuteGetStatus(
    const base::DictionaryValue& params,
    const std::string& session_id,
    const CommandCallback& callback);

struct NewSessionParams {
  NewSessionParams(Log* log,
                   SessionThreadMap* session_thread_map,
                   scoped_refptr<URLRequestContextGetter> context_getter,
                   const SyncWebSocketFactory& socket_factory,
                   DeviceManager* device_manager);
  ~NewSessionParams();

  Log* log;
  SessionThreadMap* session_thread_map;
  scoped_refptr<URLRequestContextGetter> context_getter;
  SyncWebSocketFactory socket_factory;
  DeviceManager* device_manager;
};

// Creates a new session.
void ExecuteNewSession(
    const NewSessionParams& bound_params,
    const base::DictionaryValue& params,
    const std::string& session_id,
    const CommandCallback& callback);

// Quits all sessions.
void ExecuteQuitAll(
    const Command& quit_command,
    SessionThreadMap* session_thread_map,
    const base::DictionaryValue& params,
    const std::string& session_id,
    const CommandCallback& callback);

typedef base::Callback<Status(
    Session* session,
    const base::DictionaryValue&,
    scoped_ptr<base::Value>*)> SessionCommand;

// Executes a given session command, after acquiring access to the appropriate
// session.
void ExecuteSessionCommand(
    SessionThreadMap* session_thread_map,
    const SessionCommand& command,
    bool return_ok_without_session,
    const base::DictionaryValue& params,
    const std::string& session_id,
    const CommandCallback& callback);

namespace internal {
void CreateSessionOnSessionThreadForTesting(const std::string& id);
}  // namespace internal

#endif  // CHROME_TEST_CHROMEDRIVER_COMMANDS_H_
