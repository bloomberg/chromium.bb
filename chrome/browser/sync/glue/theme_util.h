// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_THEME_UTIL_H_
#define CHROME_BROWSER_SYNC_GLUE_THEME_UTIL_H_
#pragma once

class Extension;
class Profile;

namespace sync_pb {
class ThemeSpecifics;
}  // sync_pb

namespace browser_sync {

extern const char kCurrentThemeClientTag[];

// Returns true iff two ThemeSpecifics indicate the same theme.
bool AreThemeSpecificsEqual(const sync_pb::ThemeSpecifics& a,
                            const sync_pb::ThemeSpecifics& b);

// Exposed only for testing.
bool AreThemeSpecificsEqualHelper(
    const sync_pb::ThemeSpecifics& a,
    const sync_pb::ThemeSpecifics& b,
    bool is_system_theme_distinct_from_default_theme);

// Sets the current theme from the information in the given
// ThemeSpecifics.
void SetCurrentThemeFromThemeSpecifics(
    const sync_pb::ThemeSpecifics& theme_specifics,
    Profile* profile);

// Sets all fields of the given ThemeSpecifics according to the
// current theme.
void GetThemeSpecificsFromCurrentTheme(
    Profile* profile,
    sync_pb::ThemeSpecifics* theme_specifics);

// Exposed only for testing.
void GetThemeSpecificsFromCurrentThemeHelper(
    const Extension* current_theme,
    bool is_system_theme_distinct_from_default_theme,
    bool use_system_theme_by_default,
    sync_pb::ThemeSpecifics* theme_specifics);

// Like SetChrrentThemeFromThemeSpecifics() except does nothing if the
// current theme is equivalent to that described by theme_specifics.
void SetCurrentThemeFromThemeSpecificsIfNecessary(
    const sync_pb::ThemeSpecifics& theme_specifics, Profile* profile);

// Like SetCurrentThemeFromThemeSpecifics() except that in the case where
// |theme_specifics| uses the default theme and |profile| does not, the local
// data "wins" against the server's and |theme_specifics| is updated with the
// custom theme. Returns true iff |theme_specifics| is updated.
bool UpdateThemeSpecificsOrSetCurrentThemeIfNecessary(
    Profile* profile, sync_pb::ThemeSpecifics* theme_specifics);

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_THEME_UTIL_H_
