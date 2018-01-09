// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_BAD_FLAGS_PROMPT_H_
#define CHROME_BROWSER_UI_STARTUP_BAD_FLAGS_PROMPT_H_

class Browser;

namespace content {
class WebContents;
}

namespace chrome {

// Shows a warning notification in |browser| that the app was run with dangerous
// command line flags.
void ShowBadFlagsPrompt(Browser* browser);

// Shows a warning about a specific flag.  Exposed publicly only for testing;
// should otherwise be used only by ShowBadFlagsPrompt().
void ShowBadFlagsInfoBar(content::WebContents* web_contents,
                         int message_id,
                         const char* flag);

// Shows a warning dialog if the originally specified user data dir was invalid.
void MaybeShowInvalidUserDataDirWarningDialog();

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_STARTUP_BAD_FLAGS_PROMPT_H_
