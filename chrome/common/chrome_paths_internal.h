// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_PATHS_INTERNAL_H_
#define CHROME_COMMON_CHROME_PATHS_INTERNAL_H_

#include <string>

#include "build/build_config.h"

#if defined(OS_MACOSX)
#if defined(__OBJC__)
@class NSBundle;
#else
class NSBundle;
#endif
#endif

class FilePath;

namespace chrome {

// Get the path to the user's data directory, regardless of whether
// DIR_USER_DATA has been overridden by a command-line option.
bool GetDefaultUserDataDirectory(FilePath* result);

#if defined(OS_WIN)
// Gets the path to the user data directory for the alternate environment to
// the one in use (metro or desktop).
bool GetAlternateUserDataDirectory(FilePath *result);
#endif

// This returns the base directory in which Chrome Frame stores user profiles.
// Note that this cannot be wrapped in a preprocessor define since
// CF and Google Chrome want to share the same binaries.
bool GetChromeFrameUserDataDirectory(FilePath* result);

// Get the path to the user's cache directory.  This is normally the
// same as the profile directory, but on Linux it can also be
// $XDG_CACHE_HOME and on Mac it can be under ~/Library/Caches.
// Note that the Chrome cache directories are actually subdirectories
// of this directory, with names like "Cache" and "Media Cache".
// This will always fill in |result| with a directory, sometimes
// just |profile_dir|.
void GetUserCacheDirectory(const FilePath& profile_dir, FilePath* result);

// Get the path to the user's documents directory.
bool GetUserDocumentsDirectory(FilePath* result);

#if defined(OS_WIN) || defined(OS_LINUX)
// Gets the path to a safe default download directory for a user.
bool GetUserDownloadsDirectorySafe(FilePath* result);
#endif

// Get the path to the user's downloads directory.
bool GetUserDownloadsDirectory(FilePath* result);

// Gets the path to the user's music directory.
bool GetUserMusicDirectory(FilePath* result);

// Gets the path to the user's pictures directory.
bool GetUserPicturesDirectory(FilePath* result);

// Gets the path to the user's videos directory.
bool GetUserVideosDirectory(FilePath* result);

// The path to the user's desktop.
bool GetUserDesktop(FilePath* result);

#if defined(OS_MACOSX)
// The "versioned directory" is a directory in the browser .app bundle.  It
// contains the bulk of the application, except for the things that the system
// requires be located at spepcific locations.  The versioned directory is
// in the .app at Contents/Versions/w.x.y.z.
FilePath GetVersionedDirectory();

// This overrides the directory returned by |GetVersionedDirectory()|, to be
// used when |GetVersionedDirectory()| can't automatically determine the proper
// location. This is the case when the browser didn't load itself but by, e.g.,
// the app mode loader. This should be called before |ChromeMain()|. This takes
// ownership of the object |path| and the caller must not delete it.
void SetOverrideVersionedDirectory(const FilePath* path);

// Most of the application is further contained within the framework.  The
// framework bundle is located within the versioned directory at a specific
// path.  The only components in the versioned directory not included in the
// framework are things that also depend on the framework, such as the helper
// app bundle.
FilePath GetFrameworkBundlePath();

// Get the local library directory.
bool GetLocalLibraryDirectory(FilePath* result);

// Get the global Application Support directory (under /Library/).
bool GetGlobalApplicationSupportDirectory(FilePath* result);

// Returns the NSBundle for the outer browser application, even when running
// inside the helper. In unbundled applications, such as tests, returns nil.
NSBundle* OuterAppBundle();

#endif  // OS_MACOSX

// Checks if the |process_type| has the rights to access the profile.
bool ProcessNeedsProfileDir(const std::string& process_type);

}  // namespace chrome

#endif  // CHROME_COMMON_CHROME_PATHS_INTERNAL_H_
