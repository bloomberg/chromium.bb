// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_SHOWN_SECTIONS_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_SHOWN_SECTIONS_HANDLER_H_
#pragma once

#include "chrome/browser/dom_ui/dom_ui.h"
#include "chrome/common/notification_observer.h"
#include "chrome/browser/prefs/pref_change_registrar.h"

class DOMUI;
class Extension;
class Value;
class PrefService;

// Use for the shown sections bitmask.
// Currently, only the THUMB and APPS sections can be toggled by the user. Other
// sections are shown automatically if they have data, and hidden otherwise.
enum Section {
  // If one of these is set, the corresponding section shows large thumbnails,
  // else it shows only a small overview list.
  THUMB = 1 << 0,
  APPS = 1 << 6,

  // We use the low 16 bits for sections, the high 16 bits for menu mode.
  ALL_SECTIONS_MASK = 0x0000FFFF,

  // If one of these is set, then the corresponding section is shown in a menu
  // at the bottom of the NTP and no data is directly visible on the NTP.
  MENU_THUMB = 1 << (0 + 16),
  MENU_RECENT = 1 << (2 + 16),
  MENU_APPS = 1 << (6 + 16),
};

class ShownSectionsHandler : public DOMMessageHandler,
                             public NotificationObserver {
 public:
  explicit ShownSectionsHandler(PrefService* pref_service);
  virtual ~ShownSectionsHandler() {}

  // Helper to get the current shown sections.
  static int GetShownSections(PrefService* pref_service);

  // Expands |section|, collapsing any previously expanded section. This is the
  // same thing that happens if a user clicks on |section|.
  static void SetShownSection(PrefService* prefs, Section section);

  // DOMMessageHandler implementation.
  virtual void RegisterMessages();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Callback for "getShownSections" message.
  void HandleGetShownSections(const ListValue* args);

  // Callback for "setShownSections" message.
  void HandleSetShownSections(const ListValue* args);

  static void RegisterUserPrefs(PrefService* pref_service);

  static void MigrateUserPrefs(PrefService* pref_service,
                               int old_pref_version,
                               int new_pref_version);

  static void OnExtensionInstalled(PrefService* prefs,
                                   const Extension* extension);

 private:
  PrefService* pref_service_;
  PrefChangeRegistrar pref_registrar_;

  DISALLOW_COPY_AND_ASSIGN(ShownSectionsHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_SHOWN_SECTIONS_HANDLER_H_
