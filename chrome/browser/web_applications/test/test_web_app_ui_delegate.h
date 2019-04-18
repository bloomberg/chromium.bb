// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_UI_DELEGATE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_UI_DELEGATE_H_

#include <map>

#include "base/macros.h"
#include "chrome/browser/web_applications/components/web_app_ui_delegate.h"

namespace web_app {

class TestWebAppUiDelegate : public WebAppUiDelegate {
 public:
  TestWebAppUiDelegate();
  virtual ~TestWebAppUiDelegate();

  void SetNumWindowsForApp(const AppId& app_id, size_t num_windows_for_app);

  // WebAppUiDelegate
  size_t GetNumWindowsForApp(const AppId& app_id) override;
  void NotifyOnAllAppWindowsClosed(const AppId& app_id,
                                   base::OnceClosure callback) override;

 private:
  std::map<AppId, size_t> app_id_to_num_windows_map_;

  DISALLOW_COPY_AND_ASSIGN(TestWebAppUiDelegate);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_UI_DELEGATE_H_
