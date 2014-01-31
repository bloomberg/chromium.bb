// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_TEST_PASSWORD_GENERATION_AGENT_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_TEST_PASSWORD_GENERATION_AGENT_H_

#include <vector>

#include "base/memory/scoped_vector.h"
#include "components/autofill/content/renderer/password_generation_agent.h"
#include "ipc/ipc_message.h"

namespace autofill {

class TestPasswordGenerationAgent : public PasswordGenerationAgent {
 public:
  explicit TestPasswordGenerationAgent(content::RenderView* render_view);
  virtual ~TestPasswordGenerationAgent();

  // content::RenderViewObserver implementation:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual bool Send(IPC::Message* message) OVERRIDE;

  // Access messages that would have been sent to the browser.
  const std::vector<IPC::Message*>& messages() const { return messages_.get(); }

  // Clear outgoing message queue.
  void clear_messages() { messages_.clear(); }

  // PasswordGenreationAgent implementation:
  // Always return true to allow loading of data URLs.
  virtual bool ShouldAnalyzeDocument(
      const blink::WebDocument& document) const OVERRIDE;

 private:
  ScopedVector<IPC::Message> messages_;

  DISALLOW_COPY_AND_ASSIGN(TestPasswordGenerationAgent);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_TEST_PASSWORD_AUTOFILL_AGENT_H_
