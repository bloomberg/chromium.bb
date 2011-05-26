// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLATFORM_UTIL_H_
#define CHROME_BROWSER_PLATFORM_UTIL_H_
#pragma once

#include <string>

#include "base/string16.h"
#include "ui/gfx/native_widget_types.h"

class FilePath;
class GURL;

namespace platform_util {

// The possible channels for an installation, from most fun to most stable.
enum Channel {
  CHANNEL_UNKNOWN = 0,  // Probably blue
  CHANNEL_CANARY,       // Yellow
  CHANNEL_DEV,          // Technicolor
  CHANNEL_BETA,         // Rainbow
  CHANNEL_STABLE        // Full-spectrum
};

// Show the given file in a file manager. If possible, select the file.
void ShowItemInFolder(const FilePath& full_path);

// Open the given file in the desktop's default manner.
void OpenItem(const FilePath& full_path);

// Open the given external protocol URL in the desktop's default manner.
// (For example, mailto: URLs in the default mail user agent.)
void OpenExternal(const GURL& url);

// Get the top level window for the native view. This can return NULL.
gfx::NativeWindow GetTopLevel(gfx::NativeView view);

// Get the direct parent of |view|, may return NULL.
gfx::NativeView GetParent(gfx::NativeView view);

// Returns true if |window| is the foreground top level window.
bool IsWindowActive(gfx::NativeWindow window);

// Activate the window, bringing it to the foreground top level.
void ActivateWindow(gfx::NativeWindow window);

// Returns true if the view is visible. The exact definition of this is
// platform-specific, but it is generally not "visible to the user", rather
// whether the view has the visible attribute set.
bool IsVisible(gfx::NativeView view);

// Pops up an error box with an OK button. If |parent| is non-null, the box
// will be modal on it. (On Mac, it is always app-modal.) Generally speaking,
// this function should not be used for much. Infobars are preferred.
void SimpleErrorBox(gfx::NativeWindow parent,
                    const string16& title,
                    const string16& message);

// Pops up a dialog box with two buttons (Yes/No), with the default button of
// Yes. If |parent| is non-null, the box will be modal on it. (On Mac, it is
// always app-modal.) Returns true if the Yes button was chosen.
bool SimpleYesNoBox(gfx::NativeWindow parent,
                    const string16& title,
                    const string16& message);

// Returns a human-readable modifier for the version string. For a branded
// build, this modifier is the channel ("canary", "dev", or "beta", but ""
// for stable). On Windows, this may be modified with additional information
// after a hyphen. For multi-user installations, it will return "canary-m",
// "dev-m", "beta-m", and for a stable channel multi-user installation, "m".
// In branded builds, when the channel cannot be determined, "unknown" will
// be returned. In unbranded builds, the modifier is usually an empty string
// (""), although on Linux, it may vary in certain distributions.
// GetVersionStringModifier() is intended to be used for display purposes.
// To simply test the channel, use GetChannel().
std::string GetVersionStringModifier();

// Returns the channel for the installation. In branded builds, this will be
// CHANNEL_STABLE, CHANNEL_BETA, CHANNEL_DEV, or CHANNEL_CANARY. In unbranded
// builds, or in branded builds when the channel cannot be determined, this
// will be CHANNEL_UNKNOWN.
Channel GetChannel();

// Returns true if the running browser can be set as the default browser.
bool CanSetAsDefaultBrowser();

// Returns true if the running browser can be set as the default client
// application for the given protocol.
bool CanSetAsDefaultProtocolClient(const std::string& protocol);

}

#endif  // CHROME_BROWSER_PLATFORM_UTIL_H_
