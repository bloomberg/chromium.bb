// Copyright (c) 2008-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_UTIL_H_
#define BASE_MAC_UTIL_H_

#include <Carbon/Carbon.h>
#include <string>
#include <vector>

class FilePath;

#ifdef __OBJC__
@class NSBundle;
@class NSWindow;
#else
class NSBundle;
class NSWindow;
#endif

namespace mac_util {

std::string PathFromFSRef(const FSRef& ref);
bool FSRefFromPath(const std::string& path, FSRef* ref);

// Returns true if the application is running from a bundle
bool AmIBundled();

// Returns true if this process is marked as a "Background only process".
bool IsBackgroundOnlyProcess();

// Returns the main bundle or the override, used for code that needs
// to fetch resources from bundles, but work within a unittest where we
// aren't a bundle.
NSBundle* MainAppBundle();
FilePath MainAppBundlePath();

// Set the bundle that MainAppBundle will return, overriding the default value
// (Restore the default by calling SetOverrideAppBundle(nil)).
void SetOverrideAppBundle(NSBundle* bundle);
void SetOverrideAppBundlePath(const FilePath& file_path);

// Returns the creator code associated with the CFBundleRef at bundle.
OSType CreatorCodeForCFBundleRef(CFBundleRef bundle);

// Returns the creator code associated with this application, by calling
// CreatorCodeForCFBundleRef for the application's main bundle.  If this
// information cannot be determined, returns kUnknownType ('????').  This
// does not respect the override app bundle because it's based on CFBundle
// instead of NSBundle, and because callers probably don't want the override
// app bundle's creator code anyway.
OSType CreatorCodeForApplication();

// Returns the ~/Library directory.
FilePath GetUserLibraryPath();

// Returns an sRGB color space.  The return value is a static value; do not
// release it!
CGColorSpaceRef GetSRGBColorSpace();

// Returns the color space being used by the main display.  The return value
// is a static value; do not release it!
CGColorSpaceRef GetSystemColorSpace();

// Add a request for full screen mode.  This does not by itself create a
// fullscreen window; rather, it manages per-application state related to
// fullscreen windows.  For example, if the menu bar is not currently
// hidden, this will hide it.  Must be called on main thread.
void RequestFullScreen();

// Release a request for full screen mode.  As with RequestFullScree(), this
// does not affect windows directly, but rather manages per-application state.
// For example, if there are no other outstanding requests for full screen,
// this will show the menu bar.  Must be called on main thread.
void ReleaseFullScreen();

// Pulls a snapshot of the entire browser into png_representation.
void GrabWindowSnapshot(NSWindow* window,
                        std::vector<unsigned char>* png_representation);
}  // namespace mac_util

#endif  // BASE_MAC_UTIL_H_
