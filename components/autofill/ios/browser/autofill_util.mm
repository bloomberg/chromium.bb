// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/ios/browser/autofill_util.h"

#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/ssl_status.h"

namespace autofill {

bool IsContextSecureForWebState(web::WebState* web_state) {
  // This implementation differs slightly from other platforms. Other platforms'
  // implementations check for the presence of active mixed content, but because
  // the iOS web view blocks active mixed content without an option to run it,
  // there is no need to check for active mixed content here.
  web::NavigationManager* manager = web_state->GetNavigationManager();
  web::NavigationItem* nav_item = manager->GetLastCommittedItem();
  if (!nav_item)
    return false;

  const web::SSLStatus& ssl = nav_item->GetSSL();
  return nav_item->GetURL().SchemeIsCryptographic() && ssl.certificate &&
         (!net::IsCertStatusError(ssl.cert_status) ||
          net::IsCertStatusMinorError(ssl.cert_status));
}

}  // namespace autofill
