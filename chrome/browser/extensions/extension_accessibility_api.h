// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_ACCESSIBILITY_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_ACCESSIBILITY_API_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "base/values.h"
#include "chrome/browser/accessibility_events.h"
#include "chrome/browser/extensions/extension_function.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

// Observes the profile and routes accessibility notifications as events
// to the extension system.
class ExtensionAccessibilityEventRouter : public content::NotificationObserver {
 public:
  // Single instance of the event router.
  static ExtensionAccessibilityEventRouter* GetInstance();

  // Get the dict representing the last control that received an
  // OnControlFocus event.
  DictionaryValue* last_focused_control_dict() {
    return &last_focused_control_dict_;
  }

  // Accessibility support is disabled until an extension expicitly enables
  // it, so that this extension api has no impact on Chrome's performance
  // otherwise.  These methods handle enabling, disabling, and querying the
  // status.
  void SetAccessibilityEnabled(bool enabled);
  bool IsAccessibilityEnabled() const;

 private:
  friend struct DefaultSingletonTraits<ExtensionAccessibilityEventRouter>;

  ExtensionAccessibilityEventRouter();
  virtual ~ExtensionAccessibilityEventRouter();

  // content::NotificationObserver::Observe.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void OnWindowOpened(const AccessibilityWindowInfo* details);
  void OnWindowClosed(const AccessibilityWindowInfo* details);
  void OnControlFocused(const AccessibilityControlInfo* details);
  void OnControlAction(const AccessibilityControlInfo* details);
  void OnTextChanged(const AccessibilityControlInfo* details);
  void OnMenuOpened(const AccessibilityMenuInfo* details);
  void OnMenuClosed(const AccessibilityMenuInfo* details);
  void OnVolumeChanged(const AccessibilityVolumeInfo* details);
  void OnScreenUnlocked(const ScreenUnlockedEventInfo* details);
  void OnWokeUp(const WokeUpEventInfo* details);

  void DispatchEvent(Profile* profile,
                     const char* event_name,
                     const std::string& json_args);

  // Used for tracking registrations to history service notifications.
  content::NotificationRegistrar registrar_;

  DictionaryValue last_focused_control_dict_;

  bool enabled_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionAccessibilityEventRouter);
};

// API function that enables or disables accessibility support.  Event
// listeners are only installed when accessibility support is enabled, to
// minimize the impact.
class SetAccessibilityEnabledFunction : public SyncExtensionFunction {
  virtual ~SetAccessibilityEnabledFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.accessibility.setAccessibilityEnabled")
};

// API function that returns the most recent focused control.
class GetFocusedControlFunction : public SyncExtensionFunction {
  virtual ~GetFocusedControlFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.accessibility.getFocusedControl")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_ACCESSIBILITY_API_H_
