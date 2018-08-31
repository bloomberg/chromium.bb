// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_WEB_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_WEB_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "components/autofill_assistant/browser/web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class MockWebController : public WebController {
 public:
  MockWebController();
  ~MockWebController() override;

  void ClickElement(const std::vector<std::string>& selectors,
                    base::OnceCallback<void(bool)> callback) override {
    // Transforming callback into a references allows using RunOnceCallback on
    // the argument.
    OnClickElement(selectors, callback);
  }
  MOCK_METHOD2(OnClickElement,
               void(const std::vector<std::string>& selectors,
                    base::OnceCallback<void(bool)>& callback));

  void ElementExists(const std::vector<std::string>& selectors,
                     base::OnceCallback<void(bool)> callback) override {
    OnElementExists(selectors, callback);
  }
  MOCK_METHOD2(OnElementExists,
               void(const std::vector<std::string>& selectors,
                    base::OnceCallback<void(bool)>& callback));
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_WEB_CONTROLLER_H_
