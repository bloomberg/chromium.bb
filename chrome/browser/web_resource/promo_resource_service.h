// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_RESOURCE_PROMO_RESOURCE_SERVICE_H_
#define CHROME_BROWSER_WEB_RESOURCE_PROMO_RESOURCE_SERVICE_H_
#pragma once

#include "chrome/browser/web_resource/web_resource_service.h"

namespace PromoResourceServiceUtil {

// Certain promotions should only be shown to certain classes of users. This
// function will change to reflect each kind of promotion.
bool CanShowPromo(Profile* profile);

}  // namespace PromoResourceServiceUtil

// A PromoResourceService fetches data from a web resource server to be used to
// dynamically change the appearance of the New Tab Page. For example, it has
// been used to fetch "tips" to be displayed on the NTP, or to display
// promotional messages to certain groups of Chrome users.
//
// TODO(mirandac): Arrange for a server to be set up specifically for promo
// messages, which have until now been piggybacked onto the old tips server
// structure. (see http://crbug.com/70634 for details.)
class PromoResourceService
    : public WebResourceService {
 public:
  explicit PromoResourceService(Profile* profile);

  // Unpack the web resource as a set of tips. Expects json in the form of:
  // {
  //   "lang": "en",
  //   "topic": {
  //     "topid_id": "24013",
  //     "topics": [
  //     ],
  //     "answers": [
  //       {
  //         "answer_id": "18625",
  //         "inproduct": "Text here will be shown as a tip",
  //       },
  //       ...
  //     ]
  //   }
  // }
  //
  // Public for unit testing.
  void UnpackTips(const DictionaryValue& parsed_json);

  // Unpack the web resource as a custom promo signal. Expects a start and end
  // signal, with the promo to be shown in the tooltip of the start signal
  // field. Delivery will be in json in the form of:
  // {
  //   "topic": {
  //     "answers": [
  //       {
  //         "answer_id": "1067976",
  //         "name": "promo_start",
  //         "question": "1:24",
  //         "tooltip":
  //       "Click \u003ca href=http://www.google.com\u003ehere\u003c/a\u003e!",
  //         "inproduct": "10/8/09 12:00",
  //         "inproduct_target": null
  //       },
  //       {
  //         "answer_id": "1067976",
  //         "name": "promo_end",
  //         "question": "",
  //         "tooltip": "",
  //         "inproduct": "10/8/11 12:00",
  //         "inproduct_target": null
  //       },
  //       ...
  //     ]
  //   }
  // }
  //
  // Because the promo signal data is piggybacked onto the tip server, the
  // values don't exactly correspond with the field names:
  //
  // For "promo_start" or "promo_end", the date to start or stop showing the
  // promotional line is given by the "inproduct" line.
  // For "promo_start", the promotional line itself is given in the "tooltip"
  // field. The "question" field gives the type of builds that should be shown
  // this promo (see the BuildType enum in web_resource_service.cc) and the
  // number of hours that each promo group should see it, separated by ":".
  // For example, "7:24" would indicate that all builds should see the promo,
  // and each group should see it for 24 hours.
  //
  // Public for unit testing.
  void UnpackPromoSignal(const DictionaryValue& parsed_json);

  // Unpack the promo resource as a custom logo signal. Expects a start and end
  // signal. Delivery will be in json in the form of:
  // {
  //   "topic": {
  //     "answers": [
  //       {
  //         "answer_id": "107366",
  //         "name": "custom_logo_start",
  //         "question": "",
  //         "tooltip": "",
  //         "inproduct": "10/8/09 12:00",
  //         "inproduct_target": null
  //       },
  //       {
  //         "answer_id": "107366",
  //         "name": "custom_logo_end",
  //         "question": "",
  //         "tooltip": "",
  //         "inproduct": "10/8/09 12:00",
  //         "inproduct_target": null
  //       },
  //       ...
  //     ]
  //   }
  // }
  //
  // Public for unit testing.
  void UnpackLogoSignal(const DictionaryValue& parsed_json);

  static const char* kCurrentTipPrefName;
  static const char* kTipCachePrefName;

  // Default server of dynamically loaded NTP HTML elements (promotions, tips):
  static const char* kDefaultPromoResourceServer;

 private:
  virtual ~PromoResourceService();

  virtual void Unpack(const DictionaryValue& parsed_json);

  void Init();

  // Schedule a notification that a web resource is either going to become
  // available or be no longer valid.
  void ScheduleNotification(double ms_start_time, double ms_end_time);

  // Gets mutable dictionary attached to user's preferences, so that we
  // can write resource data back to user's pref file.
  DictionaryValue* web_resource_cache_;

  DISALLOW_COPY_AND_ASSIGN(PromoResourceService);
};

#endif  // CHROME_BROWSER_WEB_RESOURCE_PROMO_RESOURCE_SERVICE_H_

