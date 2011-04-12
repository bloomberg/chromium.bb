// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants for the event names sent to extensions.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_EVENT_NAMES_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_EVENT_NAMES_H_
#pragma once

namespace extension_event_names {

// Tabs.
extern const char kOnTabAttached[];
extern const char kOnTabCreated[];
extern const char kOnTabDetached[];
extern const char kOnTabMoved[];
extern const char kOnTabRemoved[];
extern const char kOnTabSelectionChanged[];
extern const char kOnTabUpdated[];

// Windows.
extern const char kOnWindowCreated[];
extern const char kOnWindowFocusedChanged[];
extern const char kOnWindowRemoved[];

// Management.
extern const char kOnExtensionInstalled[];
extern const char kOnExtensionUninstalled[];
extern const char kOnExtensionEnabled[];
extern const char kOnExtensionDisabled[];

// FileBrowser.
extern const char kOnFileBrowserDiskChanged[];

};  // namespace extension_event_names

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_EVENT_NAMES_H_
