// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_TEST_PASSWORD_GENERATION_AGENT_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_TEST_PASSWORD_GENERATION_AGENT_H_

#include <vector>

#include "base/macros.h"
#include "components/autofill/content/renderer/password_generation_agent.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace autofill {

class TestPasswordGenerationAgent : public PasswordGenerationAgent {
 public:
  TestPasswordGenerationAgent(content::RenderFrame* render_frame,
                              PasswordAutofillAgent* password_agent,
                              blink::AssociatedInterfaceRegistry* registry);
  ~TestPasswordGenerationAgent() override;

  // PasswordGenreationAgent implementation:
  // Always return true to allow loading of data URLs.
  bool ShouldAnalyzeDocument() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestPasswordGenerationAgent);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_TEST_PASSWORD_AUTOFILL_AGENT_H_
