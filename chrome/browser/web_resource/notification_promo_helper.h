// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_RESOURCE_NOTIFICATION_PROMO_HELPER_H_
#define CHROME_BROWSER_WEB_RESOURCE_NOTIFICATION_PROMO_HELPER_H_

#include "components/web_resource/notification_promo.h"

// Helpers for NewTabPageHandler.
namespace web_resource {

// Mark the promo as closed when the user dismisses it.
void HandleNotificationPromoClosed(NotificationPromo::PromoType promo_type);

// Mark the promo has having been viewed. This returns true if views
// exceeds the maximum allowed.
bool HandleNotificationPromoViewed(NotificationPromo::PromoType promo_type);

}  // namespace web_resource

#endif  // CHROME_BROWSER_WEB_RESOURCE_NOTIFICATION_PROMO_HELPER_H_
