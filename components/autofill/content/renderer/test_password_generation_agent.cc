// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/test_password_generation_agent.h"

namespace autofill {

TestPasswordGenerationAgent::TestPasswordGenerationAgent(
    content::RenderView* render_view)
    : PasswordGenerationAgent(render_view) {
  // Always enable when testing.
  set_enabled(true);
}

TestPasswordGenerationAgent::~TestPasswordGenerationAgent() {}

bool TestPasswordGenerationAgent::OnMessageReceived(
    const IPC::Message& message) {
  return PasswordGenerationAgent::OnMessageReceived(message);
}

bool TestPasswordGenerationAgent::ShouldAnalyzeDocument(
    const blink::WebDocument& document) const {
  return true;
}

bool TestPasswordGenerationAgent::Send(IPC::Message* message) {
  messages_.push_back(message);
  return true;
}

}  // namespace autofill
