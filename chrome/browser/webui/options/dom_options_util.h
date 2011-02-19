// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBUI_OPTIONS_DOM_OPTIONS_UTIL_H_
#define CHROME_BROWSER_WEBUI_OPTIONS_DOM_OPTIONS_UTIL_H_
#pragma once

#include "base/string16.h"

namespace dom_options_util {

// Strips the trailing colon from a string. Used to remove the colon from
// section titles on platforms that append it.
// TODO(jhawkins): Remove this once the platform-specific options dialogs are
// removed.
string16 StripColon(const string16& str);

}  // namespace options_util

#endif  // CHROME_BROWSER_WEBUI_OPTIONS_DOM_OPTIONS_UTIL_H_
