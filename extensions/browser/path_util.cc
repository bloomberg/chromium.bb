// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/path_util.h"

#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "build/build_config.h"

#if defined(OS_MACOSX)
#include <CoreFoundation/CoreFoundation.h>
#include "base/mac/foundation_util.h"
#endif

namespace extensions {
namespace path_util {

namespace {
#if defined(OS_MACOSX)

// Retrieves the localized display name for the base name of the given path.
// If the path is not localized, this will just return the base name.
std::string GetDisplayBaseName(const base::FilePath& path) {
  base::ScopedCFTypeRef<CFURLRef> url(CFURLCreateFromFileSystemRepresentation(
      NULL, (const UInt8*)path.value().c_str(), path.value().length(), true));
  if (!url)
    return path.BaseName().value();

  CFStringRef str;
  if (LSCopyDisplayNameForURL(url, &str) != noErr)
    return path.BaseName().value();

  std::string result(base::SysCFStringRefToUTF8(str));
  CFRelease(str);
  return result;
}

#endif  // defined(OS_MACOSX)

const base::FilePath::CharType kHomeShortcut[] = FILE_PATH_LITERAL("~");

}  // namespace

base::FilePath PrettifyPath(const base::FilePath& source_path) {
  base::FilePath home_path;
  if (source_path.empty() || !PathService::Get(base::DIR_HOME, &home_path))
    return source_path;

  base::FilePath display_path = base::FilePath(kHomeShortcut);
  if (source_path == home_path)
    return display_path;

#if defined(OS_MACOSX)
  DCHECK(source_path.IsAbsolute());

  // Break down the incoming path into components, and grab the display name
  // for every component. This will match app bundles, ".localized" folders,
  // and localized subfolders of the user's home directory.
  // Don't grab the display name of the first component, i.e., "/", as it'll
  // show up as the HDD name.
  std::vector<base::FilePath::StringType> components;
  source_path.GetComponents(&components);
  display_path = base::FilePath(components[0]);
  base::FilePath actual_path = display_path;
  for (std::vector<base::FilePath::StringType>::iterator i =
           components.begin() + 1; i != components.end(); ++i) {
    actual_path = actual_path.Append(*i);
    if (actual_path == home_path) {
      display_path = base::FilePath(kHomeShortcut);
      home_path = base::FilePath();
      continue;
    }
    std::string display = GetDisplayBaseName(actual_path);
    display_path = display_path.Append(display);
  }
  DCHECK_EQ(actual_path.value(), source_path.value());
  return display_path;
#else  // defined(OS_MACOSX)
  if (home_path.AppendRelativePath(source_path, &display_path))
    return display_path;
  return source_path;
#endif  // defined(OS_MACOSX)
}

}  // namespace path_util
}  // namespace extensions
