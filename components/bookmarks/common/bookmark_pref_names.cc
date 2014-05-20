// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/common/bookmark_pref_names.h"

namespace prefs {

// Boolean which specifies the ids of the bookmark nodes that are expanded in
// the bookmark editor.
const char kBookmarkEditorExpandedNodes[] = "bookmark_editor.expanded_nodes";

// Boolean which specifies whether the bookmark bar is visible on all tabs.
const char kShowBookmarkBar[] = "bookmark_bar.show_on_all_tabs";

// Modifying bookmarks is completely disabled when this is set to false.
const char kEditBookmarksEnabled[] = "bookmarks.editing_enabled";

// Boolean which specifies whether the apps shortcut is visible on the bookmark
// bar.
const char kShowAppsShortcutInBookmarkBar[] = "bookmark_bar.show_apps_shortcut";

}  // namespace prefs
