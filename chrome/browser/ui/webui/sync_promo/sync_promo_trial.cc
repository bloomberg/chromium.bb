// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_promo/sync_promo_trial.h"

#include "base/logging.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/ui/webui/sync_promo/sync_promo_ui.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

namespace {

enum {
  UMA_START_PAGE_SHOWN = 0,
  UMA_START_PAGE_SIGNED_IN,
  UMA_NTP_LINK_SHOWN,
  UMA_NTP_LINK_SIGNED_IN,
  UMA_MENU_SHOWN,
  UMA_MENU_SIGNED_IN,
  UMA_UNKNOWN_SHOWN,
  UMA_UNKNOWN_SIGNED_IN,
  UMA_MAX,
};

}  // namespace

namespace sync_promo_trial {

void RecordUserShownPromo(content::WebUI* web_ui) {
  SyncPromoUI::Source source = SyncPromoUI::GetSourceForSyncPromoURL(
        web_ui->GetWebContents()->GetURL());
  int uma = 0;
  switch (source) {
    case SyncPromoUI::SOURCE_START_PAGE:
      uma = UMA_START_PAGE_SHOWN;
      break;
    case SyncPromoUI::SOURCE_NTP_LINK:
      uma = UMA_NTP_LINK_SHOWN;
      break;
    case SyncPromoUI::SOURCE_MENU:
      uma = UMA_MENU_SHOWN;
      break;
    case SyncPromoUI::SOURCE_UNKNOWN:
      uma = UMA_UNKNOWN_SHOWN;
      break;
    default:
      NOTREACHED();
      break;
  }
  UMA_HISTOGRAM_ENUMERATION("SyncPromo.ShowAndSignIn", uma, UMA_MAX);
}

void RecordUserSignedIn(content::WebUI* web_ui) {
  SyncPromoUI::Source source = SyncPromoUI::GetSourceForSyncPromoURL(
        web_ui->GetWebContents()->GetURL());
  int uma = 0;
  switch (source) {
    case SyncPromoUI::SOURCE_START_PAGE:
      uma = UMA_START_PAGE_SIGNED_IN;
      break;
    case SyncPromoUI::SOURCE_NTP_LINK:
      uma = UMA_NTP_LINK_SIGNED_IN;
      break;
    case SyncPromoUI::SOURCE_MENU:
      uma = UMA_MENU_SIGNED_IN;
      break;
    case SyncPromoUI::SOURCE_UNKNOWN:
      uma = UMA_UNKNOWN_SIGNED_IN;
      break;
    default:
      NOTREACHED();
      break;
  }
  UMA_HISTOGRAM_ENUMERATION("SyncPromo.ShowAndSignIn", uma, UMA_MAX);
}

}  // namespace sync_promo_trial
