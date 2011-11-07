// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper classes and functions used for the WebRequest API.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBREQUEST_API_HELPERS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBREQUEST_API_HELPERS_H_
#pragma once

#include<string>

namespace base {
class ListValue;
}

namespace extension_webrequest_api_helpers {

// Converts a string to a list of integers, each in 0..255. Ownership
// of the created list is passed to the caller.
base::ListValue* StringToCharList(const std::string& s);

// Converts a list of integer values between 0 and 255 into a string |*out|.
// Returns true if the conversion was successful.
bool CharListToString(base::ListValue* list, std::string* out);

}  // namespace extension_webrequest_api_helpers

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBREQUEST_API_HELPERS_H_
