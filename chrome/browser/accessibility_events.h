// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_EVENTS_H_
#define CHROME_BROWSER_ACCESSIBILITY_EVENTS_H_
#pragma once

#include <string>
#include "base/compiler_specific.h"

class AccessibilityEventInfo;
class Profile;

namespace base {
class DictionaryValue;
}

// Use the NotificationService to post the given accessibility
// notification type with AccessibilityEventInfo details to any
// listeners.  Will not send if the profile's pause level is nonzero
// (using profile->PauseAccessibilityEvents).
void SendAccessibilityNotification(
    int type, AccessibilityEventInfo* info);
void SendAccessibilityVolumeNotification(double volume, bool is_muted);

// Abstract parent class for accessibility event information passed to event
// listeners.
class AccessibilityEventInfo {
 public:
  virtual ~AccessibilityEventInfo() {}

  // Serialize this class as a DictionaryValue that can be converted to
  // a JavaScript object.
  virtual void SerializeToDict(base::DictionaryValue* dict) const = 0;

  Profile* profile() const { return profile_; }

 protected:
  explicit AccessibilityEventInfo(Profile* profile) : profile_(profile) {}

  // The profile this control belongs to.
  Profile* profile_;
};

// Abstract parent class for accessibility information about a control
// passed to event listeners.
class AccessibilityControlInfo : public AccessibilityEventInfo {
 public:
  virtual ~AccessibilityControlInfo();

  // Serialize this class as a DictionaryValue that can be converted to
  // a JavaScript object.
  virtual void SerializeToDict(base::DictionaryValue* dict) const OVERRIDE;

  // Return the specific type of this control, which will be one of the
  // string constants defined in extension_accessibility_api_constants.h.
  virtual const char* type() const = 0;

  const std::string& name() const { return name_; }

 protected:
  AccessibilityControlInfo(Profile* profile, const std::string& control_name);

  // The name of the control, like "OK" or "Password".
  std::string name_;
};

// Accessibility information about a window passed to onWindowOpened
// and onWindowClosed event listeners.
class AccessibilityWindowInfo : public AccessibilityControlInfo {
 public:
  AccessibilityWindowInfo(Profile* profile, const std::string& window_name);

  virtual const char* type() const OVERRIDE;
};

// Accessibility information about a push button passed to onControlFocused
// and onControlAction event listeners.
class AccessibilityButtonInfo : public AccessibilityControlInfo {
 public:
  AccessibilityButtonInfo(Profile* profile, const std::string& button_name);

  virtual const char* type() const OVERRIDE;
};

// Accessibility information about a hyperlink passed to onControlFocused
// and onControlAction event listeners.
class AccessibilityLinkInfo : public AccessibilityControlInfo {
 public:
  AccessibilityLinkInfo(Profile* profile, const std::string& link_name);

  virtual const char* type() const OVERRIDE;
};

// Accessibility information about a radio button passed to onControlFocused
// and onControlAction event listeners.
class AccessibilityRadioButtonInfo : public AccessibilityControlInfo {
 public:
  AccessibilityRadioButtonInfo(Profile* profile,
                               const std::string& name,
                               bool checked,
                               int item_index,
                               int item_count);

  virtual const char* type() const OVERRIDE;

  virtual void SerializeToDict(base::DictionaryValue* dict) const OVERRIDE;

  void SetChecked(bool checked) { checked_ = checked; }

  int item_index() const { return item_index_; }
  int item_count() const { return item_count_; }
  bool checked() const { return checked_; }

 private:
  bool checked_;
  // The 0-based index of this radio button and number of buttons in the group.
  int item_index_;
  int item_count_;
};

// Accessibility information about a checkbox passed to onControlFocused
// and onControlAction event listeners.
class AccessibilityCheckboxInfo : public AccessibilityControlInfo {
 public:
  AccessibilityCheckboxInfo(Profile* profile,
                            const std::string& name,
                            bool checked);

  virtual const char* type() const OVERRIDE;

  virtual void SerializeToDict(base::DictionaryValue* dict) const OVERRIDE;

  void SetChecked(bool checked) { checked_ = checked; }

  bool checked() const { return checked_; }

 private:
  bool checked_;
};

// Accessibility information about a tab passed to onControlFocused
// and onControlAction event listeners.
class AccessibilityTabInfo : public AccessibilityControlInfo {
 public:
  AccessibilityTabInfo(Profile* profile,
                       const std::string& tab_name,
                       int tab_index,
                       int tab_count);

  virtual const char* type() const OVERRIDE;

  virtual void SerializeToDict(base::DictionaryValue* dict) const OVERRIDE;

  void SetTab(int tab_index, std::string tab_name) {
    tab_index_ = tab_index;
    name_ = tab_name;
  }

  int tab_index() const { return tab_index_; }
  int tab_count() const { return tab_count_; }

 private:
  // The 0-based index of this tab and number of tabs in the group.
  int tab_index_;
  int tab_count_;
};

// Accessibility information about a combo box passed to onControlFocused
// and onControlAction event listeners.
class AccessibilityComboBoxInfo : public AccessibilityControlInfo {
 public:
  AccessibilityComboBoxInfo(Profile* profile,
                            const std::string& name,
                            const std::string& value,
                            int item_index,
                            int item_count);

  virtual const char* type() const OVERRIDE;

  virtual void SerializeToDict(base::DictionaryValue* dict) const OVERRIDE;

  void SetValue(int item_index, const std::string& value) {
    item_index_ = item_index;
    value_ = value;
  }

  int item_index() const { return item_index_; }
  int item_count() const { return item_count_; }
  const std::string& value() const { return value_; }

 private:
  std::string value_;
  // The 0-based index of the current item and the number of total items.
  // If the value is not one of the drop-down options, |item_index_| should
  // be -1.
  int item_index_;
  int item_count_;
};

// Accessibility information about a text box, passed to onControlFocused,
// onControlAction, and onTextChanged event listeners.
class AccessibilityTextBoxInfo : public AccessibilityControlInfo {
 public:
  AccessibilityTextBoxInfo(Profile* profile,
                           const std::string& name,
                           bool password);

  virtual const char* type() const OVERRIDE;

  virtual void SerializeToDict(base::DictionaryValue* dict) const OVERRIDE;

  void SetValue(
      const std::string& value, int selection_start, int selection_end) {
    value_ = value;
    selection_start_ = selection_start;
    selection_end_ = selection_end;
  }

  const std::string& value() const { return value_; }
  bool password() const { return password_; }
  int selection_start() const { return selection_start_; }
  int selection_end() const { return selection_end_; }

 private:
  std::string value_;
  bool password_;
  int selection_start_;
  int selection_end_;
};

// Accessibility information about a combo box passed to onControlFocused
// and onControlAction event listeners.
class AccessibilityListBoxInfo : public AccessibilityControlInfo {
 public:
  AccessibilityListBoxInfo(Profile* profile,
                           const std::string& name,
                           const std::string& value,
                           int item_index,
                           int item_count);

  virtual const char* type() const OVERRIDE;

  virtual void SerializeToDict(base::DictionaryValue* dict) const OVERRIDE;

  void SetValue(int item_index, std::string value) {
    item_index_ = item_index;
    value_ = value;
  }

  int item_index() const { return item_index_; }
  int item_count() const { return item_count_; }
  const std::string& value() const { return value_; }

 private:
  std::string value_;
  // The 0-based index of the current item and the number of total items.
  // If the value is not one of the drop-down options, |item_index_| should
  // be -1.
  int item_index_;
  int item_count_;
};

// Accessibility information about a menu; this class is used by
// onMenuOpened, onMenuClosed, and onControlFocused event listeners.
class AccessibilityMenuInfo : public AccessibilityControlInfo {
 public:
  AccessibilityMenuInfo(Profile* profile, const std::string& menu_name);

  virtual const char* type() const OVERRIDE;
};

// Accessibility information about a volume; this class is used by
// onVolumeUp, onVolumeDown, and onVolumeMute event listeners.
class AccessibilityVolumeInfo : public AccessibilityEventInfo {
 public:
  // |volume| must range between 0 to 100.
  AccessibilityVolumeInfo(Profile* profile, double volume, bool is_muted);

  virtual void SerializeToDict(base::DictionaryValue* dict) const OVERRIDE;

 private:
  double volume_;
  bool is_muted_;
};

// Screen unlock event information; this class is used by onScreenUnlocked.
class ScreenUnlockedEventInfo : public AccessibilityEventInfo {
 public:
  ScreenUnlockedEventInfo(Profile* profile);
  virtual void SerializeToDict(base::DictionaryValue* dict) const OVERRIDE;
};

// Wake up event information; this class is used by onWokeUp.
class WokeUpEventInfo : public AccessibilityEventInfo {
 public:
  WokeUpEventInfo(Profile* profile);
  virtual void SerializeToDict(base::DictionaryValue* dict) const OVERRIDE;
};

// Accessibility information about a menu item; this class is used by
// onControlFocused event listeners.
class AccessibilityMenuItemInfo : public AccessibilityControlInfo {
 public:
  AccessibilityMenuItemInfo(Profile* profile,
                            const std::string& name,
                            bool has_submenu,
                            int item_index,
                            int item_count);

  virtual const char* type() const OVERRIDE;

  virtual void SerializeToDict(base::DictionaryValue* dict) const OVERRIDE;

  int item_index() const { return item_index_; }
  int item_count() const { return item_count_; }
  bool has_submenu() const { return has_submenu_; }

 private:
  bool has_submenu_;
  // The 0-based index of the current item and the number of total items.
  int item_index_;
  int item_count_;
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_EVENTS_H_
