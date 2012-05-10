// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_BAD_FLAGS_PROMPT_H_
#define CHROME_BROWSER_UI_STARTUP_BAD_FLAGS_PROMPT_H_
#pragma once

class Browser;

namespace browser {

// Shows a warning notification in |browser| that the app was run with dangerous
// command line flags.
void ShowBadFlagsPrompt(Browser* browser);

}

#endif  // CHROME_BROWSER_UI_STARTUP_BAD_FLAGS_PROMPT_H_
