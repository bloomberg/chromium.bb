// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_SESSION_CRASHED_PROMPT_H_
#define CHROME_BROWSER_UI_STARTUP_SESSION_CRASHED_PROMPT_H_
#pragma once

class Browser;

namespace chrome {

// Shows the session crashed prompt for |browser| if required.
void ShowSessionCrashedPrompt(Browser* browser);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_STARTUP_SESSION_CRASHED_PROMPT_H_
