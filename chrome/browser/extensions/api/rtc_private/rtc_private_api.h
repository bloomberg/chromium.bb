// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_EXTENSIONS_API_RTC_PRIVATE_RTC_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_RTC_PRIVATE_RTC_PRIVATE_API_H_

class Profile;

namespace contacts {
class Contact;
}

namespace extensions {

// Implementation of chrome.rtcPrivate API.
class RtcPrivateEventRouter {
 public:
  // Contact launch intent action.
  enum LaunchAction {
    LAUNCH_ACTIVATE,
    LAUNCH_CHAT,
    LAUNCH_VIDEO,
    LAUNCH_VOICE,
  };

  // Raises chrome.rtcPrivate.onLaunch event which is used to launch the default
  // RTC application in ChromeOS. |action| defines the context of the launch
  // event. Optional |contact| information is passed as event payload if launch
  // event is related to a particilar contact.
  static void DispatchLaunchEvent(Profile* profile,
                                  LaunchAction action,
                                  contacts::Contact* contact);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_RTC_PRIVATE_RTC_PRIVATE_API_H_
