// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_USDZ_MIME_TYPE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_USDZ_MIME_TYPE_H_

#include <string>

// Universal Scene Description file format used to represent 3D models.
// See https://www.iana.org/assignments/media-types/model/vnd.usdz+zip
extern char kUsdzMimeType[];
// Legacy USDZ content types.
extern char kLegacyUsdzMimeType[];
extern char kLegacyPixarUsdzMimeType[];

bool IsUsdzFileFormat(const std::string& mime_type);

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_USDZ_MIME_TYPE_H_
