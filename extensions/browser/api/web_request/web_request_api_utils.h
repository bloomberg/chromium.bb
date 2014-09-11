// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper functions used for the WebRequest API.

#ifndef EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_API_UTILS_H_
#define EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_API_UTILS_H_

#include <string>

#include "content/public/common/resource_type.h"

using content::ResourceType;

namespace extension_web_request_api_utils {

// Returns whether |type| is a ResourceType that is handled by the web request
// API.
bool IsRelevantResourceType(content::ResourceType type);

// Returns a string representation of |type| or |other| if |type| is not handled
// by the web request API.
const char* ResourceTypeToString(content::ResourceType type);

// Stores a |content::ResourceType| representation in |type| if |type_str| is
// a resource type handled by the web request API. Returns true in case of
// success.
bool ParseResourceType(const std::string& type_str,
                       content::ResourceType* type);

}  // namespace extension_web_request_api_utils

#endif  // EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_API_UTILS_H_
