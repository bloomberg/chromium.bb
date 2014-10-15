// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines various defaults whose values varies depending upon the OS.

#ifndef CHROME_BROWSER_DEFAULTS_H_
#define CHROME_BROWSER_DEFAULTS_H_

#include "build/build_config.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "ui/base/resource/resource_bundle.h"

namespace browser_defaults {

#if defined(USE_X11)

// Can the user toggle the system title bar?
extern const bool kCanToggleSystemTitleBar;

#endif

// Size of the font used in the omnibox, in pixels.
extern const int kOmniboxFontPixelSize;

// Width of mini-tabs.  Inline because other constants are computed depending
// on this, which causes static initializers if the value isn't available in
// all translation units that do this.
#if defined(TOOLKIT_VIEWS)
// Windows and Chrome OS have bigger shadows in the tab art.
const int kMiniTabWidth = 64;
#else
const int kMiniTabWidth = 56;
#endif

#if defined(TOOLKIT_VIEWS)
// Whether to show a Link Disambiguation Popup Bubble if the browser detects an
// ambiguous touch event.
extern const bool kShowLinkDisambiguationPopup;
#endif

// Can the browser be alive without any browser windows?
extern const bool kBrowserAliveWithNoWindows;

// Whether various menu items are shown.
extern const bool kShowExitMenuItem;
extern const bool kShowHelpMenuItemIcon;
extern const bool kShowUpgradeMenuItem;

// Should a link be shown on the bookmark bar allowing the user to import
// bookmarks?
extern const bool kShowImportOnBookmarkBar;

// Should always open incognito windows when started with --incognito switch?
extern const bool kAlwaysOpenIncognitoWindow;

// Indicates whether session restore should always create a new
// tabbed browser. This is true every where except on ChromeOS
// where we want the desktop to show through in this situation.
extern const bool kAlwaysCreateTabbedBrowserOnSessionRestore;

// Does the download page have the show in folder option?
extern const bool kDownloadPageHasShowInFolder;

// Should the tab strip be sized to the top of the tab strip?
extern const bool kSizeTabButtonToTopOfTabStrip;

// If true, we want to automatically start sync signin whenever we have
// credentials (user doesn't need to go through the startup flow). This is
// typically enabled on platforms (like ChromeOS) that have their own
// distinct signin flow.
extern const bool kSyncAutoStarts;

// Should other browsers be shown in about:memory page?
extern const bool kShowOtherBrowsersInAboutMemory;

// Should scroll events on the tabstrip change tabs?
extern const bool kScrollEventChangesTab;

// ChromiumOS network menu font
extern const ui::ResourceBundle::FontStyle kAssociatedNetworkFontStyle;

// Last character display for passwords.
extern const bool kPasswordEchoEnabled;

//=============================================================================
// Runtime "const" - set only once after parsing command line option and should
// never be modified after that.

// Are bookmark enabled? True by default.
extern bool bookmarks_enabled;

// Whether HelpApp is enabled. True by default. This is only used by Chrome OS
// today.
extern bool enable_help_app;

}  // namespace browser_defaults

#endif  // CHROME_BROWSER_DEFAULTS_H_
