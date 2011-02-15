// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEBUI_LOGIN_MOCK_LOGIN_UI_HELPERS_H_
#define CHROME_BROWSER_CHROMEOS_WEBUI_LOGIN_MOCK_LOGIN_UI_HELPERS_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/webui/login/login_ui_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockProfileOperationsInterface
    : public chromeos::ProfileOperationsInterface {
 public:
  MockProfileOperationsInterface() {}

  MOCK_METHOD0(GetDefaultProfile,
               Profile*());
  MOCK_METHOD0(GetDefaultProfileByPath,
               Profile*());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockProfileOperationsInterface);
};

class MockBrowserOperationsInterface
    : public chromeos::BrowserOperationsInterface {
 public:
  MockBrowserOperationsInterface() {}

  MOCK_METHOD1(CreateBrowser,
               Browser*(Profile* profile));
  MOCK_METHOD1(GetLoginBrowser,
               Browser*(Profile* profile));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBrowserOperationsInterface);
};

class MockHTMLOperationsInterface
    : public chromeos::HTMLOperationsInterface {
 public:
  MockHTMLOperationsInterface() {}

  MOCK_METHOD0(GetLoginHTML,
               base::StringPiece());
  MOCK_METHOD2(GetFullHTML,
               std::string(base::StringPiece login_html,
                           DictionaryValue* localized_strings));
  MOCK_METHOD1(CreateHTMLBytes,
               RefCountedBytes*(std::string full_html));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockHTMLOperationsInterface);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WEBUI_LOGIN_MOCK_LOGIN_UI_HELPERS_H_
