// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CONVERT_WEB_APP_H_
#define CHROME_BROWSER_EXTENSIONS_CONVERT_WEB_APP_H_
#pragma once

#include <string>

#include "base/memory/ref_counted.h"

class Extension;

namespace base {
class Time;
}

struct WebApplicationInfo;

// Generates a version number for an extension from a time. The goal is to make
// use of the version number to communicate the date in a human readable form,
// while maintaining high enough resolution to change each time an app is
// reinstalled. The version that is returned has the format:
//
// <year>.<month>.<day>.<fraction>
//
// fraction is represented as a number between 0 and 2^16-1. Each unit is
// ~1.32 seconds.
std::string ConvertTimeToExtensionVersion(const base::Time& time);

// Wraps the specified web app in an extension. The extension is created
// unpacked in the system temp dir. Returns a valid extension that the caller
// should take ownership on success, or NULL and |error| on failure.
//
// NOTE: This function does file IO and should not be called on the UI thread.
// NOTE: The caller takes ownership of the directory at extension->path() on the
// returned object.
scoped_refptr<Extension> ConvertWebAppToExtension(
    const WebApplicationInfo& web_app_info,
    const base::Time& create_time);

#endif  // CHROME_BROWSER_EXTENSIONS_CONVERT_WEB_APP_H_
