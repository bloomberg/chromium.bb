// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHARACTER_ENCODING_H_
#define CHROME_BROWSER_CHARACTER_ENCODING_H_

#include <string>

// Return canonical encoding name according to the encoding alias name.
// TODO(jinsukkim): Move this to somewhere under content/.
std::string GetCanonicalEncodingNameByAliasName(const std::string& alias_name);

#endif  // CHROME_BROWSER_CHARACTER_ENCODING_H_
