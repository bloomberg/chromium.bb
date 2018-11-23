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
struct RectF;

class MockWebController : public WebController {
 public:
  MockWebController();
  ~MockWebController() override;

  MOCK_METHOD0(GetUrl, const GURL&());

  MOCK_METHOD1(LoadURL, void(const GURL&));

  void ClickOrTapElement(const Selector& selector,
                         base::OnceCallback<void(bool)> callback) override {
    // Transforming callback into a references allows using RunOnceCallback on
    // the argument.
    OnClickOrTapElement(selector, callback);
  }
  MOCK_METHOD2(OnClickOrTapElement,
               void(const Selector& selector,
                    base::OnceCallback<void(bool)>& callback));

  void FocusElement(const Selector& selector,
                    base::OnceCallback<void(bool)> callback) override {
    OnFocusElement(selector, callback);
  }
  MOCK_METHOD2(OnFocusElement,
               void(const Selector& selector,
                    base::OnceCallback<void(bool)>& callback));

  void ElementCheck(ElementCheckType check_type,
                    const Selector& selector,
                    base::OnceCallback<void(bool)> callback) override {
    OnElementCheck(check_type, selector, callback);
  }
  MOCK_METHOD3(OnElementCheck,
               void(ElementCheckType check_type,
                    const Selector& selector,
                    base::OnceCallback<void(bool)>& callback));

  void GetFieldValue(
      const Selector& selector,
      base::OnceCallback<void(bool, const std::string&)> callback) override {
    OnGetFieldValue(selector, callback);
  }
  MOCK_METHOD2(
      OnGetFieldValue,
      void(const Selector& selector,
           base::OnceCallback<void(bool, const std::string&)>& callback));

  void GetElementPosition(
      const Selector& selector,
      base::OnceCallback<void(bool, const RectF&)> callback) override {
    OnGetElementPosition(selector, callback);
  }
  MOCK_METHOD2(OnGetElementPosition,
               void(const Selector& selector,
                    base::OnceCallback<void(bool, const RectF&)>& callback));

  void HasCookie(base::OnceCallback<void(bool)> callback) override {
    std::move(callback).Run(false);
  }

  void SetCookie(const std::string& domain,
                 base::OnceCallback<void(bool)> callback) override {
    std::move(callback).Run(true);
  }

  MOCK_METHOD0(ClearCookie, void());
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_WEB_CONTROLLER_H_
