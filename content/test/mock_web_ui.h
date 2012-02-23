// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_WEB_UI_H_
#define CONTENT_TEST_MOCK_WEB_UI_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "content/public/browser/web_ui.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class MockWebUI : public content::WebUI {
 public:
  MockWebUI();
  virtual ~MockWebUI();
  MOCK_CONST_METHOD0(GetWebContents, WebContents*());
  MOCK_CONST_METHOD0(GetController, WebUIController*());
  MOCK_METHOD1(SetController, void(WebUIController*));
  MOCK_CONST_METHOD0(ShouldHideFavicon, bool());
  MOCK_METHOD0(HideFavicon, void());
  MOCK_CONST_METHOD0(ShouldFocusLocationBarByDefault, bool());
  MOCK_METHOD0(FocusLocationBarByDefault, void());
  MOCK_CONST_METHOD0(ShouldHideURL, bool());
  MOCK_METHOD0(HideURL, void());
  MOCK_CONST_METHOD0(GetOverriddenTitle, const string16&());
  MOCK_METHOD1(OverrideTitle, void(const string16&));
  MOCK_CONST_METHOD0(GetLinkTransitionType, PageTransition());
  MOCK_METHOD1(SetLinkTransitionType, void(PageTransition));
  MOCK_CONST_METHOD0(GetBindings, int());
  MOCK_METHOD1(SetBindings, void(int));
  MOCK_METHOD1(SetFrameXPath, void(const std::string&));
  MOCK_METHOD1(AddMessageHandler, void(WebUIMessageHandler*));
  MOCK_METHOD2(RegisterMessageCallback, void(const std::string&,
                                             const MessageCallback&));
  MOCK_METHOD3(ProcessWebUIMessage, void(const GURL&,
                                         const std::string&,
                                         const base::ListValue&));
  MOCK_METHOD1(CallJavascriptFunction, void(const std::string&));
  MOCK_METHOD2(CallJavascriptFunction, void(const std::string&,
                                            const base::Value&));
  MOCK_METHOD3(CallJavascriptFunction, void(const std::string&,
                                            const base::Value&,
                                            const base::Value&));
  MOCK_METHOD4(CallJavascriptFunction, void(const std::string&,
                                            const base::Value&,
                                            const base::Value&,
                                            const base::Value&));
  MOCK_METHOD5(CallJavascriptFunction, void(const std::string&,
                                            const base::Value&,
                                            const base::Value&,
                                            const base::Value&,
                                            const base::Value&));
  MOCK_METHOD2(CallJavascriptFunction,
               void(const std::string&,
                    const std::vector<const base::Value*>&));
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_WEB_UI_H_
