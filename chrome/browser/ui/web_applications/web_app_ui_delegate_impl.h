// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_DELEGATE_IMPL_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_DELEGATE_IMPL_H_

#include "base/macros.h"
#include "chrome/browser/web_applications/components/web_app_ui_delegate.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace web_app {

class WebAppUiDelegateImpl : public KeyedService, public WebAppUiDelegate {
 public:
  static WebAppUiDelegateImpl* Get(Profile* profile);

  explicit WebAppUiDelegateImpl(Profile* profile);
  ~WebAppUiDelegateImpl() override;

  // WebAppUiDelegate
  size_t GetNumWindowsForApp(const AppId& app_id) override;

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(WebAppUiDelegateImpl);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_DELEGATE_IMPL_H_
