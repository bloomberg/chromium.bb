// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_resource/notification_promo_helper.h"

#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/browser_process.h"
#include "content/public/browser/user_metrics.h"

namespace web_resource {

void HandleNotificationPromoClosed(NotificationPromo::PromoType promo_type) {
  content::RecordAction(base::UserMetricsAction("NTPPromoClosed"));
  NotificationPromo::HandleClosed(promo_type, g_browser_process->local_state());
}

bool HandleNotificationPromoViewed(NotificationPromo::PromoType promo_type) {
  content::RecordAction(base::UserMetricsAction("NTPPromoShown"));
  return NotificationPromo::HandleViewed(promo_type,
                                         g_browser_process->local_state());
}

}  // namespace web_resource
