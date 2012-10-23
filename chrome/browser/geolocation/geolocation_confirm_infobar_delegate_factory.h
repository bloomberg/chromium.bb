// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_CONFIRM_INFOBAR_DELEGATE_FACTORY_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_CONFIRM_INFOBAR_DELEGATE_FACTORY_H_

#include "base/values.h"

class GeolocationConfirmInfoBarDelegate;
class GeolocationInfoBarQueueController;
class GURL;
class InfoBarTabHelper;

class GeolocationConfirmInfoBarDelegateFactory {
 public:
  GeolocationConfirmInfoBarDelegateFactory() {}
  ~GeolocationConfirmInfoBarDelegateFactory() {}
  static GeolocationConfirmInfoBarDelegate* Create(
      InfoBarTabHelper* infobar_helper,
      GeolocationInfoBarQueueController* controller,
      int render_process_id,
      int render_view_id,
      int bridge_id,
      const GURL& requesting_frame_url,
      const std::string& display_languages);

 private:

  DISALLOW_COPY_AND_ASSIGN(GeolocationConfirmInfoBarDelegateFactory);
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_CONFIRM_INFOBAR_DELEGATE_FACTORY_H_
