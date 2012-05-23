// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/automation_events.h"

ScriptEvaluationRequest::ScriptEvaluationRequest() {
}

ScriptEvaluationRequest::ScriptEvaluationRequest(
    const std::string& script,
    const std::string& frame_xpath)
    : script(script),
      frame_xpath(frame_xpath) {
}

ScriptEvaluationRequest::~ScriptEvaluationRequest() {
}

AutomationMouseEvent::AutomationMouseEvent() {
}

AutomationMouseEvent::~AutomationMouseEvent() {
}
