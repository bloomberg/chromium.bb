// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_COMMAND_EXECUTOR_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_COMMAND_EXECUTOR_IMPL_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "chrome/test/chromedriver/command_executor.h"
#include "chrome/test/chromedriver/status.h"

namespace base {
class DictionaryValue;
class Value;
}

class CommandExecutorImpl : public CommandExecutor {
 public:
  CommandExecutorImpl();
  virtual ~CommandExecutorImpl();

  // Overriden from CommandExecutor:
  virtual void ExecuteCommand(const std::string& name,
                              const base::DictionaryValue& params,
                              const std::string& session_id,
                              StatusCode* status_code,
                              scoped_ptr<base::Value>* value,
                              std::string* out_session_id) OVERRIDE;

 private:
  typedef base::Callback<void(
      const base::DictionaryValue& params,
      Status* status,
      scoped_ptr<base::Value>* value)>
          SessionCommandCallback;
  typedef std::map<std::string, SessionCommandCallback> SessionCommandMap;

  void ExecuteCommandInternal(
      const std::string& name,
      const base::DictionaryValue& params,
      const std::string& session_id,
      Status* status,
      scoped_ptr<base::Value>* value,
      std::string* out_session_id);

  void ExecuteNewSession(std::string* session_id);

  base::Lock lock_;
  int next_session_id_;
  SessionCommandMap session_command_map_;

  DISALLOW_COPY_AND_ASSIGN(CommandExecutorImpl);
};

#endif  // CHROME_TEST_CHROMEDRIVER_COMMAND_EXECUTOR_IMPL_H_
