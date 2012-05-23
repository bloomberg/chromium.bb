// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_AUTOMATION_EVENTS_H_
#define CHROME_COMMON_AUTOMATION_EVENTS_H_
#pragma once

#include <string>
#include <vector>

#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"

// A request to evaluate a script in a given frame.
struct ScriptEvaluationRequest {
  ScriptEvaluationRequest();
  ScriptEvaluationRequest(const std::string& script,
                          const std::string& frame_xpath);
  ~ScriptEvaluationRequest();

  std::string script;
  std::string frame_xpath;
};

// An extension to a normal mouse event that includes data for determining the
// location of the mouse event.
struct AutomationMouseEvent {
  AutomationMouseEvent();
  ~AutomationMouseEvent();

  WebKit::WebMouseEvent mouse_event;

  // A list of scripts that when evaluated returns a coordinate for the event.
  // The scripts are chained together in that the output for one script
  // provides the input for the next. Each script must result in exactly one
  // JavaScript object. This object will be passed to the next script
  // as arguments[0]. The only exceptions to this are the first script, which
  // is passed null. The last script should result in a single JavaScript
  // object with integer 'x' and 'y' properties.
  // If empty, the coordinates in the normal mouse event are used.
  std::vector<ScriptEvaluationRequest> location_script_chain;
};

#endif  // CHROME_COMMON_AUTOMATION_EVENTS_H_
