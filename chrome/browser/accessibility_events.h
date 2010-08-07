// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_EVENTS_H_
#define CHROME_BROWSER_ACCESSIBILITY_EVENTS_H_
#pragma once

#include <string>

class AccessibilityControlInfo;
class DictionaryValue;
class NotificationType;
class Profile;

// Use the NotificationService to post the given accessibility
// notification type with AccessibilityControlInfo details to any
// listeners.  Will not send if the profile's pause level is nonzero
// (using profile->PauseAccessibilityEvents).
void SendAccessibilityNotification(
    NotificationType type, AccessibilityControlInfo* info);

// Abstract parent class for accessibility information about a control
// passed to event listeners.
class AccessibilityControlInfo {
 public:
  virtual ~AccessibilityControlInfo() { }

  // Serialize this class as a DictionaryValue that can be converted to
  // a JavaScript object.
  virtual void SerializeToDict(DictionaryValue* dict) const;

  // Return the specific type of this control, which will be one of the
  // string constants defined in extension_accessibility_api_constants.h.
  virtual const char* type() const = 0;

  Profile* profile() const { return profile_; }

  const std::string& name() const { return name_; }

 protected:
  // The constructor can only be called by subclasses.
  AccessibilityControlInfo(Profile* profile, std::string control_name)
      : profile_(profile), name_(control_name) { }

  // The profile this control belongs to.
  Profile* profile_;

  // The name of the control, like "OK" or "Password".
  std::string name_;
};

// Accessibility information about a window passed to onWindowOpened
// and onWindowClosed event listeners.
class AccessibilityWindowInfo : public AccessibilityControlInfo {
 public:
  AccessibilityWindowInfo(Profile* profile, std::string window_name)
      : AccessibilityControlInfo(profile, window_name) { }

  virtual const char* type() const;
};

// Accessibility information about a push button passed to onControlFocused
// and onControlAction event listeners.
class AccessibilityButtonInfo : public AccessibilityControlInfo {
 public:
  AccessibilityButtonInfo(Profile* profile, std::string button_name)
      : AccessibilityControlInfo(profile, button_name) { }

  virtual const char* type() const;
};

// Accessibility information about a hyperlink passed to onControlFocused
// and onControlAction event listeners.
class AccessibilityLinkInfo : public AccessibilityControlInfo {
 public:
  AccessibilityLinkInfo(Profile* profile, std::string link_name)
      : AccessibilityControlInfo(profile, link_name) { }

  virtual const char* type() const;
};

// Accessibility information about a radio button passed to onControlFocused
// and onControlAction event listeners.
class AccessibilityRadioButtonInfo : public AccessibilityControlInfo {
 public:
  AccessibilityRadioButtonInfo(Profile* profile,
                               std::string name,
                               bool checked,
                               int item_index,
                               int item_count)
      : AccessibilityControlInfo(profile, name),
        checked_(checked),
        item_index_(item_index),
        item_count_(item_count) {
  }

  virtual const char* type() const;

  virtual void SerializeToDict(DictionaryValue* dict) const;

  void SetChecked(bool checked) { checked_ = checked; }

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
                            std::string name,
                            bool checked)
      : AccessibilityControlInfo(profile, name),
        checked_(checked) {
  }

  virtual const char* type() const;

  virtual void SerializeToDict(DictionaryValue* dict) const;

  void SetChecked(bool checked) { checked_ = checked; }

 private:
  bool checked_;
};

// Accessibility information about a tab passed to onControlFocused
// and onControlAction event listeners.
class AccessibilityTabInfo : public AccessibilityControlInfo {
 public:
  AccessibilityTabInfo(Profile* profile,
                       std::string tab_name,
                       int tab_index,
                       int tab_count)
      : AccessibilityControlInfo(profile, tab_name),
        tab_index_(tab_index),
        tab_count_(tab_count) {
  }

  virtual const char* type() const;

  virtual void SerializeToDict(DictionaryValue* dict) const;

  void SetTab(int tab_index, std::string tab_name) {
    tab_index_ = tab_index;
    name_ = tab_name;
  }

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
                            std::string name,
                            std::string value,
                            int item_index,
                            int item_count)
      : AccessibilityControlInfo(profile, name),
        value_(value),
        item_index_(item_index),
        item_count_(item_count) {
  }

  virtual const char* type() const;

  virtual void SerializeToDict(DictionaryValue* dict) const;

  void SetValue(int item_index, std::string value) {
    item_index_ = item_index;
    value_ = value;
  }

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
                           std::string name,
                           bool password)
      : AccessibilityControlInfo(profile, name),
        value_(""),
        password_(password),
        selection_start_(0),
        selection_end_(0) {
  }

  virtual const char* type() const;

  virtual void SerializeToDict(DictionaryValue* dict) const;

  void SetValue(std::string value, int selection_start, int selection_end) {
    value_ = value;
    selection_start_ = selection_start;
    selection_end_ = selection_end;
  }

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
                           std::string name,
                           std::string value,
                           int item_index,
                           int item_count)
      : AccessibilityControlInfo(profile, name),
        value_(value),
        item_index_(item_index),
        item_count_(item_count) {
  }

  virtual const char* type() const;

  virtual void SerializeToDict(DictionaryValue* dict) const;

  void SetValue(int item_index, std::string value) {
    item_index_ = item_index;
    value_ = value;
  }

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
  AccessibilityMenuInfo(Profile* profile, std::string menu_name)
      : AccessibilityControlInfo(profile, menu_name) { }

  virtual const char* type() const;
};

// Accessibility information about a menu item; this class is used by
// onControlFocused event listeners.
class AccessibilityMenuItemInfo : public AccessibilityControlInfo {
 public:
  AccessibilityMenuItemInfo(Profile* profile,
                            std::string name,
                            bool has_submenu,
                            int item_index,
                            int item_count)
      : AccessibilityControlInfo(profile, name),
        has_submenu_(has_submenu),
        item_index_(item_index),
        item_count_(item_count) {
  }

  virtual const char* type() const;

  virtual void SerializeToDict(DictionaryValue* dict) const;

 private:
  bool has_submenu_;
  // The 0-based index of the current item and the number of total items.
  int item_index_;
  int item_count_;
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_EVENTS_H_
