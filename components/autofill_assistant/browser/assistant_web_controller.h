// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_WEB_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_WEB_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"

namespace autofill_assistant {
// Controller to interact with the web pages.
class AssistantWebController {
 public:
  AssistantWebController();
  ~AssistantWebController();

  // Perform a moust left button click on the element given by |selectors| and
  // return the result through callback.
  // CSS selectors in |selectors| are ordered from top frame to the frame
  // contains the element and the element.
  void ClickElement(const std::vector<std::string>& selectors,
                    base::OnceCallback<void(bool)> callback);

  // Check whether the element given by |selectors| exists on the web page.
  void ElementExists(const std::vector<std::string>& selectors,
                     base::OnceCallback<void(bool)> callback);

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantWebController);
};

}  // namespace autofill_assistant.
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_WEB_CONTROLLER_H_
