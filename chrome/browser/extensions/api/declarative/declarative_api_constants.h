// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the Declarative API.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_DECLARATIVE_API_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_DECLARATIVE_API_CONSTANTS_H_
#pragma once

namespace extensions {
namespace declarative_api_constants {

// Keys.
extern const char kId[];
extern const char kConditions[];
extern const char kActions[];
extern const char kPriority[];

// Error messages.
extern const char kInvalidDatatype[];
extern const char kInvalidEventName[];

}  // namespace declarative_api_constants
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_DECLARATIVE_API_CONSTANTS_H_
