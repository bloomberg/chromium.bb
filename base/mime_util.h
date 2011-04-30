// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MIME_UTIL_H_
#define BASE_MIME_UTIL_H_
#pragma once

#include <string>

class FilePath;

namespace mime_util {

// Gets the mime type for a file based on its filename. The file path does not
// have to exist. Please note because it doesn't touch the disk, this does not
// work for directories.
// If the mime type is unknown, this will return application/octet-stream.
std::string GetFileMimeType(const FilePath& filepath);

// Get the mime type for a byte vector.
std::string GetDataMimeType(const std::string& data);

#if defined(TOOLKIT_GTK)
// This detects the current GTK theme by calling gtk_settings_get_default().
// It should only be executed on the UI thread and must be called before
// GetMimeIcon().
void DetectGtkTheme();
#endif

// Gets the file name for an icon given the mime type and icon pixel size.
// Where an icon is a square image of |size| x |size|.
// This will try to find the closest matching icon. If that's not available,
// then a generic icon, and finally an empty FilePath if all else fails.
FilePath GetMimeIcon(const std::string& mime_type, size_t size);

}  // namespace mime_util

#endif  // BASE_MIME_UTIL_H_
