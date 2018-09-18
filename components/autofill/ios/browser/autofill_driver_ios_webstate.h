// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_WEBSTATE_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_WEBSTATE_H_

#include "components/autofill/ios/browser/autofill_driver_ios.h"

namespace web {
class WebState;
}  // namespace web

namespace autofill {

// TODO(crbug.com/883203): remove class once WebFrame is released.
class AutofillDriverIOSWebState
    : public AutofillDriverIOS,
      public web::WebStateUserData<AutofillDriverIOSWebState> {
 public:
  static void CreateForWebStateAndDelegate(
      web::WebState* web_state,
      AutofillClient* client,
      id<AutofillDriverIOSBridge> bridge,
      const std::string& app_locale,
      AutofillManager::AutofillDownloadManagerState enable_download_manager);
  static AutofillClient* ClientForWebState(web::WebState* web_state);

  ~AutofillDriverIOSWebState() override;

 protected:
  AutofillDriverIOSWebState(
      web::WebState* web_state,
      AutofillClient* client,
      id<AutofillDriverIOSBridge> bridge,
      const std::string& app_locale,
      AutofillManager::AutofillDownloadManagerState enable_download_manager);
};
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_AUTOFILL_DRIVER_IOS_WEBSTATE_H_
