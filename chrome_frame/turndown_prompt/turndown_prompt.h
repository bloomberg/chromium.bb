// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TURNDOWN_PROMPT_TURNDOWN_PROMPT_H_
#define CHROME_FRAME_TURNDOWN_PROMPT_TURNDOWN_PROMPT_H_

#include <atlbase.h>
#include <atlcom.h>

interface IWebBrowser2;

// Integrates the Turndown prompt functionality with a specified IWebBrowser2
// instance.  Displays prompts informing the user that Chrome Frame is being
// turned down.
namespace turndown_prompt {

// Returns true if the Turndown prompt is suppressed. Suppression may be
// explicit via the SuppressChromeFrameTurndownPrompt GPO or implicit by virtue
// of Chrome Frame having been installed via the .MSI or by updates to Chrome
// Frame having been disabled.
bool IsPromptSuppressed();

// Configures |web_browser| for the turndown prompt if the prompt has not been
// suppressed.
void Configure(IWebBrowser2* web_browser);

};  // namespace turndown_prompt

#endif  // CHROME_FRAME_TURNDOWN_PROMPT_TURNDOWN_PROMPT_H_
