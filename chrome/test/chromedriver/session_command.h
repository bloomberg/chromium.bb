// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_SESSION_COMMAND_H_
#define CHROME_TEST_CHROMEDRIVER_SESSION_COMMAND_H_

#include <string>

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/test/chromedriver/session_map.h"

namespace base {
class DictionaryValue;
class Value;
}

struct Session;
class Status;

typedef base::Callback<Status(
    Session* session,
    const base::DictionaryValue&,
    scoped_ptr<base::Value>*)> SessionCommand;

// Executes a given session command, after acquiring access to the appropriate
// session.
Status ExecuteSessionCommand(
    SessionMap* session_map,
    const SessionCommand& command,
    const base::DictionaryValue& params,
    const std::string& session_id,
    scoped_ptr<base::Value>* out_value,
    std::string* out_session_id);

#endif  // CHROME_TEST_CHROMEDRIVER_SESSION_COMMAND_H_
