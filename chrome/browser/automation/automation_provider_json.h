// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Support utilities for the JSON automation interface used by PyAuto.
// Automation call is initiated by SendJSONRequest() in browser_proxy.h which
// is serviced by handlers in automation_provider.cc and the resulting
// success/error reply is sent using AutomationJSONReply defined here.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_PROVIDER_JSON_H_
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_PROVIDER_JSON_H_

#include <string>

#include "base/values.h"
#include "ipc/ipc_message.h"
#include "chrome/browser/automation/automation_provider.h"

// Represent a reply on the JSON Automation interface used by PyAuto.
class AutomationJSONReply {

 public:
  // Creates a new reply object for the IPC message |reply_message| for
  // |provider|.
  AutomationJSONReply(AutomationProvider* provider,
                      IPC::Message* reply_message);

  ~AutomationJSONReply();

  // Send a success reply along with data contained in |value|.
  // An empty message will be sent if |value| is NULL.
  void SendSuccess(const Value* value);

  // Send an error reply along with error message |error_mesg|.
  void SendError(const std::string& error_mesg);

 private:
  // Util for creating a JSON error return string (dict with key
  // 'error' and error string value).  No need to quote input.
  static std::string JSONErrorString(const std::string& err);

  AutomationProvider* provider_;
  IPC::Message* message_;
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_PROVIDER_JSON_H_

