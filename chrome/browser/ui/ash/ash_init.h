// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASH_INIT_H_
#define CHROME_BROWSER_UI_ASH_ASH_INIT_H_

namespace browser {

// Returns true if Ash should be run at startup.
bool ShouldOpenAshOnStartup();

// Creates the Ash Shell and opens the Ash window.
void OpenAsh();

// Closes the Ash window and destroys the Ash Shell.
void CloseAsh();

}  // namespace browser

#endif  // CHROME_BROWSER_UI_ASH_ASH_INIT_H_
