// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_AUTOFILL_PASSWORD_GENERATION_TEST_UTILS_H_
#define CHROME_RENDERER_AUTOFILL_PASSWORD_GENERATION_TEST_UTILS_H_

#include <string>

namespace blink {
class WebDocument;
}

namespace autofill {

class TestPasswordGenerationAgent;

void SetNotBlacklistedMessage(TestPasswordGenerationAgent* generation_agent,
                              const char* form_str);
void SetAccountCreationFormsDetectedMessage(TestPasswordGenerationAgent* agent,
                                            blink::WebDocument document,
                                            int form_index);
}  // namespace autofill

#endif  // CHROME_RENDERER_AUTOFILL_PASSWORD_GENERATION_TEST_UTILS_H_
