// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/ios/browser/autofill_driver_ios_webstate.h"

#include "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {

// static
void AutofillDriverIOSWebState::CreateForWebStateAndDelegate(
    web::WebState* web_state,
    AutofillClient* client,
    id<AutofillDriverIOSBridge> bridge,
    const std::string& app_locale,
    AutofillManager::AutofillDownloadManagerState enable_download_manager) {
  if (FromWebState(web_state))
    return;

  web_state->SetUserData(
      UserDataKey(),
      base::WrapUnique(new AutofillDriverIOSWebState(
          web_state, client, bridge, app_locale, enable_download_manager)));
}

AutofillClient* AutofillDriverIOSWebState::ClientForWebState(
    web::WebState* web_state) {
  return AutofillDriverIOSWebState::FromWebState(web_state)
      ->autofill_manager()
      ->client();
}

AutofillDriverIOSWebState::AutofillDriverIOSWebState(
    web::WebState* web_state,
    AutofillClient* client,
    id<AutofillDriverIOSBridge> bridge,
    const std::string& app_locale,
    AutofillManager::AutofillDownloadManagerState enable_download_manager)
    : AutofillDriverIOS(web_state,
                        client,
                        bridge,
                        app_locale,
                        enable_download_manager) {}

AutofillDriverIOSWebState::~AutofillDriverIOSWebState() {}

}  //  namespace autofill
