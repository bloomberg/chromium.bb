// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_USER_SELECTABLE_SYNC_TYPE_H_
#define CHROME_BROWSER_UI_WEBUI_USER_SELECTABLE_SYNC_TYPE_H_
#pragma once

// An enumeration of the sync datatypes that are explicitly exposed to the user
// via checkboxes in the "Advanced Sync Preferences" dialog. Used solely for the
// purposes of UMA histogram logging of the datatypes explicitly selected by
// users when sync is configured on a machine. This is a subset of the sync
// types listed in chrome/browser/sync/syncable/model_type.h.
//
// Note: New sync datatypes must be added to the end of this list. Adding them
// anywhere else will result in incorrect histogram logging.

enum UserSelectableSyncType {
  BOOKMARKS = 0,
  PREFERENCES = 1,
  PASSWORDS = 2,
  AUTOFILL = 3,
  THEMES = 4,
  TYPED_URLS = 5,
  EXTENSIONS = 6,
  SESSIONS = 7,
  APPS = 8,

  // The datatypes below are implicitly synced, and are not exposed via user
  // selectable checkboxes.

  // AUTOFILL_PROFILE,
  // NIGORI,
  // SEARCH_ENGINES,
  // APP_SETTINGS,
  // EXTENSION_SETTINGS,
  // APP_NOTIFICATIONS,

  // Number of sync datatypes exposed to the user via checboxes in the UI.
  SELECTABLE_DATATYPE_COUNT = 9,
};

#endif  // CHROME_BROWSER_UI_WEBUI_USER_SELECTABLE_SYNC_TYPE_H_
