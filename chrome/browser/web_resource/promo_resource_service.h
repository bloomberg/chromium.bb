// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_RESOURCE_PROMO_RESOURCE_SERVICE_H_
#define CHROME_BROWSER_WEB_RESOURCE_PROMO_RESOURCE_SERVICE_H_
#pragma once

#include <string>

#include "chrome/browser/web_resource/web_resource_service.h"
#include "chrome/common/chrome_version_info.h"

namespace base {
  class DictionaryValue;
}

class AppsPromoLogoFetcher;
class PrefService;
class Profile;
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
  // Checks for conditions to show promo: start/end times, channel, etc.
  static bool CanShowNotificationPromo(Profile* profile);

  static void RegisterPrefs(PrefService* local_state);

  static void RegisterUserPrefs(PrefService* prefs);

  explicit PromoResourceService(Profile* profile);

  // Default server of dynamically loaded NTP HTML elements.
  static const char* kDefaultPromoResourceServer;

 private:
  FRIEND_TEST_ALL_PREFIXES(PromoResourceServiceTest, IsBuildTargeted);
  FRIEND_TEST_ALL_PREFIXES(PromoResourceServiceTest, UnpackLogoSignal);
  FRIEND_TEST_ALL_PREFIXES(PromoResourceServiceTest, UnpackNotificationSignal);
  FRIEND_TEST_ALL_PREFIXES(PromoResourceServiceTest, UnpackWebStoreSignal);
  FRIEND_TEST_ALL_PREFIXES(
      PromoResourceServiceTest, UnpackPartialWebStoreSignal);
  FRIEND_TEST_ALL_PREFIXES(
      PromoResourceServiceTest, UnpackWebStoreSignalHttpsLogo);
  FRIEND_TEST_ALL_PREFIXES(
      PromoResourceServiceTest, UnpackWebStoreSignalHttpsLogoError);
  FRIEND_TEST_ALL_PREFIXES(
      PromoResourceServiceTest, UnpackWebStoreSignalHttpLogo);

  // Identifies types of Chrome builds for promo targeting.
  enum BuildType {
    NO_BUILD = 0,
    DEV_BUILD = 1,
    BETA_BUILD = 1 << 1,
    STABLE_BUILD = 1 << 2,
    CANARY_BUILD = 1 << 3,
    ALL_BUILDS = (1 << 4) - 1,
  };

  virtual ~PromoResourceService();

  int GetPromoServiceVersion();

  // Gets the locale of the last promos fetched from the server. This is saved
  // so we can fetch new data if the locale changes.
  std::string GetPromoLocale();

  void Init();

  static bool IsBuildTargeted(chrome::VersionInfo::Channel channel,
                              int builds_targeted);

  // Returns true if |builds_targeted| includes the release channel Chrome
  // belongs to. For testing purposes, you can override the current channel
  // with set_channel.
  bool IsThisBuildTargeted(int builds_targeted);

  // Schedule a notification that a web resource is either going to become
  // available or be no longer valid.
  void ScheduleNotification(double ms_start_time, double ms_end_time);

  // Schedules the initial notification for when the web resource is going
  // to become available or no longer valid. This performs a few additional
  // checks than ScheduleNotification, namely it schedules updates immediately
  // if the promo service or Chrome locale has changed.
  void ScheduleNotificationOnInit();

  // Overrides the current Chrome release channel for testing purposes.
  void set_channel(chrome::VersionInfo::Channel channel) { channel_ = channel; }

  virtual void Unpack(const base::DictionaryValue& parsed_json);

  // Unpack the web resource as a custom promo signal. Expects a start and end
  // signal, with the promo to be shown in the tooltip of the start signal
  // field. Delivery will be in json in the form of:
  // {
  //   "topic": {
  //     "answers": [
  //       {
  //         "answer_id": "1067976",
  //         "name": "promo_start",
  //         "question": "1:24:10",
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
  // this promo (see the BuildType enum in web_resource_service.cc), the
  // number of hours that each promo group should see it, and the maximum promo
  // group that should see it, separated by ":".
  // For example, "7:24:5" would indicate that all groups with ids less than 5,
  // and with dev, beta and stable builds, should see the promo. The groups
  // ramp up so 1 additional group sees the promo every 24 hours.
  //
  void UnpackNotificationSignal(const base::DictionaryValue& parsed_json);

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
  void UnpackLogoSignal(const base::DictionaryValue& parsed_json);

  // Unpack the web store promo. Expects JSON delivery in the following format:
  // {
  //   "topic": {
  //     "answers": [
  //       {
  //         "answer_id": "1143011",
  //         "name": "webstore_promo:15:1:https://www.google.com/logo.png",
  //         "question": "Browse thousands of apps and games for Chrome.",
  //         "inproduct_target": "Visit the Chrome Web Store",
  //         "inproduct": "https://chrome.google.com/webstore?hl=en",
  //         "tooltip": "No thanks, hide this"
  //       },
  //       ...
  //     ]
  //   }
  // }
  // The properties are defined as follows:
  //   inproduct: the release channels targeted (bitwise or of BuildTypes)
  //   question: the promo header text
  //   inproduct_target: the promo button text
  //   inproduct: the promo button link
  //   tooltip: the text for the "hide this" link on the promo
  //   name: starts with "webstore_promo" to identify the signal. The second
  //         part contains the release channels targeted (bitwise or of
  //         BuildTypes). The third part specifies what users should maximize
  //         the apps section of the NTP when first loading the promo (bitwise
  //         or of AppsPromo::UserGroup). The forth part is optional and
  //         specifies the URL of the logo image. If left out, the default
  //         webstore logo will be used. The logo can be an HTTPS or DATA URL.
  //   answer_id: the promo's id
  void UnpackWebStoreSignal(const base::DictionaryValue& parsed_json);

  // Parse the answers array element.
  void ParseNotification(base::DictionaryValue* a_dic,
                         std::string* promo_start_string,
                         std::string* promo_end_string);

  // Set notification promo params from a question string, which is of the form
  // <build_type>:<time_slice>:<max_group>.
  void SetNotificationParams(base::DictionaryValue* a_dic);

  // Extract the notification promo text from the tooltip string.
  void SetNotificationLine(base::DictionaryValue* a_dic);

  // Check if this notification promo is new based on start/end times,
  // and trigger events accordingly.
  void CheckForNewNotification(const std::string& promo_start_string,
                               const std::string& promo_end_string);

  // Calculate the notification promo times, taking into account our group, and
  // the group time slice.
  void ParseNewNotificationTimes(const std::string& promo_start_string,
                                 const std::string& promo_end_string,
                                 double* promo_start,
                                 double* promo_end);

  // Calculates notification promo start time with group-based time slice
  // offset.
  static double GetNotificationStartTime(PrefService* prefs);

  // Create a new notification promo group.
  int ResetNotificationGroup();

  // Get saved notification promo times.
  void GetCurrentNotificationTimes(double* old_promo_start,
                                   double* old_promo_end);

  // Actions on receiving a new notification promo.
  void OnNewNotification(double promo_start, double promo_end);

  // The profile this service belongs to.
  Profile* profile_;

  // Overrides the current Chrome release channel for testing purposes.
  chrome::VersionInfo::Channel channel_;

  // A helper that downloads the promo logo.
  scoped_ptr<AppsPromoLogoFetcher> apps_promo_logo_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(PromoResourceService);
};

#endif  // CHROME_BROWSER_WEB_RESOURCE_PROMO_RESOURCE_SERVICE_H_
