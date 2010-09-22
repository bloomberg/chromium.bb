// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Support utilities for the JSON automation interface used by PyAuto.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_PROVIDER_JSON_H_
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_PROVIDER_JSON_H_
#pragma once

#include <string>

class Value;
class AutomationProvider;

namespace IPC {
class Message;
}

// Helper to ensure we always send a reply message for JSON automation requests.
class AutomationJSONReply {
 public:
  // Creates a new reply object for the IPC message |reply_message| for
  // |provider|. The caller is expected to call SendSuccess() or SendError()
  // before destroying this object.
  AutomationJSONReply(AutomationProvider* provider,
                      IPC::Message* reply_message);

  ~AutomationJSONReply();

  // Send a success reply along with data contained in |value|.
  // An empty message will be sent if |value| is NULL.
  void SendSuccess(const Value* value);

  // Send an error reply along with error message |error_message|.
  void SendError(const std::string& error_message);

 private:
  AutomationProvider* provider_;
  IPC::Message* message_;
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_PROVIDER_JSON_H_
