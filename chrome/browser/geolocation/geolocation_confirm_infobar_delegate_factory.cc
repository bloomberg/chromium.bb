// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_confirm_infobar_delegate_factory.h"

#if defined(OS_ANDROID)
#include "chrome/browser/geolocation/geolocation_confirm_infobar_delegate_android.h"
#else
#include "chrome/browser/geolocation/geolocation_confirm_infobar_delegate.h"
#endif

GeolocationConfirmInfoBarDelegate*
    GeolocationConfirmInfoBarDelegateFactory::Create(
        InfoBarTabHelper* infobar_helper,
        GeolocationInfoBarQueueController* controller,
        int render_process_id,
        int render_view_id,
        int bridge_id,
        const GURL& requesting_frame_url,
        const std::string& display_languages) {
#if defined(OS_ANDROID)
  return new GeolocationConfirmInfoBarDelegateAndroid(
      infobar_helper,
      controller,
      render_process_id,
      render_view_id,
      bridge_id,
      requesting_frame_url,
      display_languages);
#else
  return new GeolocationConfirmInfoBarDelegate(
      infobar_helper,
      controller,
      render_process_id,
      render_view_id,
      bridge_id,
      requesting_frame_url,
      display_languages);
#endif
}
