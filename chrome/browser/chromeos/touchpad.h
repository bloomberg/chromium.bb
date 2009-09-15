// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_TOUCHPAD_H_
#define CHROME_BROWSER_CHROMEOS_TOUCHPAD_H_

#include <string>

#include "chrome/common/notification_observer.h"
#include "chrome/common/pref_member.h"

class PrefService;

// The Touchpad class handles touchpad preferences. When the class is first
// initialized, it will initialize the touchpad settings to what's stored in the
// preferences. And whenever the preference changes, we change the touchpad
// setting to reflect the new value.
//
// For Synaptics touchpads, we use synclient to change settings on-the-fly.
// See "man synaptics" for a list of settings that can be changed
// Since we are doing a system call to run the synclient binary, we make sure
// that we are running on the IO thread so that we don't block the UI thread.
class Touchpad : public NotificationObserver {
 public:
  explicit Touchpad() {}
  virtual ~Touchpad() {}

  // This method will register the prefs associated with touchpad settings.
  static void RegisterUserPrefs(PrefService* prefs);

  // This method will initialize touchpad settings to values in user prefs.
  virtual void Init(PrefService* prefs);

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 protected:
  virtual void NotifyPrefChanged(const std::wstring* pref_name);

 private:
  // This methods makes a system call to synclient to change touchpad settings.
  // The system call will be invoked on the file thread.
  void SetSynclientParam(const std::string& param, const std::string& value);

  void SetTapToClick();
  void SetVertEdgeScroll();

  BooleanPrefMember tap_to_click_enabled_;
  BooleanPrefMember vert_edge_scroll_enabled_;

  DISALLOW_COPY_AND_ASSIGN(Touchpad);
};

#endif  // CHROME_BROWSER_CHROMEOS_TOUCHPAD_H_
