// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_SESSION_COMMANDS_H_
#define CHROME_TEST_CHROMEDRIVER_SESSION_COMMANDS_H_

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
class WebView;

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

// Quits a particular session.
Status ExecuteQuit(
    SessionMap* session_map,
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Retrieve the handle of the target window.
Status ExecuteGetCurrentWindowHandle(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Close the target window.
Status ExecuteClose(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Retrieve the list of all window handles available to the session.
Status ExecuteGetWindowHandles(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Change target window to another. The window to target at may be specified by
// its server assigned window handle, or by the value of its name attribute.
Status ExecuteSwitchToWindow(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Configure the amount of time that a particular type of operation can execute
// for before they are aborted and a timeout error is returned to the client.
Status ExecuteSetTimeout(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Returns whether an alert is open.
Status ExecuteGetAlert(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Returns the text of the open alert.
Status ExecuteGetAlertText(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Sets the value of the alert prompt.
Status ExecuteSetAlertValue(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Accepts the open alert.
Status ExecuteAcceptAlert(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

// Dismisses the open alert.
Status ExecuteDismissAlert(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value);

#endif  // CHROME_TEST_CHROMEDRIVER_SESSION_COMMANDS_H_
