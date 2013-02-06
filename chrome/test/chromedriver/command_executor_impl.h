// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_COMMAND_EXECUTOR_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_COMMAND_EXECUTOR_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "chrome/test/chromedriver/command.h"
#include "chrome/test/chromedriver/command_executor.h"
#include "chrome/test/chromedriver/net/sync_websocket_factory.h"
#include "chrome/test/chromedriver/session_map.h"
#include "chrome/test/chromedriver/status.h"
#include "chrome/test/chromedriver/synchronized_map.h"

namespace base {
class DictionaryValue;
class Value;
}

class ChromeLauncherImpl;
class URLRequestContextGetter;

class CommandExecutorImpl : public CommandExecutor {
 public:
  CommandExecutorImpl();
  virtual ~CommandExecutorImpl();

  // Overridden from CommandExecutor:
  virtual void Init() OVERRIDE;
  virtual void ExecuteCommand(const std::string& name,
                              const base::DictionaryValue& params,
                              const std::string& session_id,
                              StatusCode* status_code,
                              scoped_ptr<base::Value>* value,
                              std::string* out_session_id) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(CommandExecutorImplTest, SimpleCommand);
  FRIEND_TEST_ALL_PREFIXES(
      CommandExecutorImplTest, CommandThatDoesntSetValueOrSessionId);
  FRIEND_TEST_ALL_PREFIXES(CommandExecutorImplTest, CommandThatReturnsError);

  base::Thread io_thread_;
  scoped_refptr<URLRequestContextGetter> context_getter_;
  SyncWebSocketFactory socket_factory_;
  SessionMap session_map_;
  SynchronizedMap<std::string, Command> command_map_;

  DISALLOW_COPY_AND_ASSIGN(CommandExecutorImpl);
};

#endif  // CHROME_TEST_CHROMEDRIVER_COMMAND_EXECUTOR_IMPL_H_
