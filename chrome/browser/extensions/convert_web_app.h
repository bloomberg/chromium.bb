// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CONVERT_WEB_APP_H_
#define CHROME_BROWSER_EXTENSIONS_CONVERT_WEB_APP_H_
#pragma once

#include <string>

#include "base/ref_counted.h"

class Extension;

namespace base {
class Time;
}

namespace webkit_glue {
struct WebApplicationInfo;
}

// Generates a version number for an extension from a time. The goal is to make
// use of the version number to communicate some useful information. The
// returned version has the format:
// <year>.<month><day>.<upper 16 bits of unix timestamp>.<lower 16 bits>
std::string ConvertTimeToExtensionVersion(const base::Time& time);

// Wraps the specified web app in an extension. The extension is created
// unpacked in the system temp dir. Returns a valid extension that the caller
// should take ownership on success, or NULL and |error| on failure.
//
// NOTE: This function does file IO and should not be called on the UI thread.
// NOTE: The caller takes ownership of the directory at extension->path() on the
// returned object.
scoped_refptr<Extension> ConvertWebAppToExtension(
    const webkit_glue::WebApplicationInfo& web_app_info,
    const base::Time& create_time);

#endif  // CHROME_BROWSER_EXTENSIONS_CONVERT_WEB_APP_H_
