// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOOLKIT_EXTRA_PARTS_H_
#define CHROME_BROWSER_TOOLKIT_EXTRA_PARTS_H_
#pragma once

#include "build/build_config.h"

class ChromeBrowserMainParts;

namespace chrome {

#if defined(TOOLKIT_GTK)
void AddGtkToolkitExtraParts(ChromeBrowserMainParts* main_parts);
#endif

#if defined(TOOLKIT_VIEWS)
void AddViewsToolkitExtraParts(ChromeBrowserMainParts* main_parts);
#endif

#if defined(USE_ASH)
void AddAshToolkitExtraParts(ChromeBrowserMainParts* main_parts);
#endif

#if defined(USE_AURA)
void AddAuraToolkitExtraParts(ChromeBrowserMainParts* main_parts);
#endif

}  // namespace chrome

#endif  // CHROME_BROWSER_TOOLKIT_EXTRA_PARTS_H_
