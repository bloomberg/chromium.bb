// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_DEFAULT_BROWSER_PROMPT_H_
#define CHROME_BROWSER_UI_DEFAULT_BROWSER_PROMPT_H_
#pragma once

class Profile;

namespace browser {

// Shows a prompt UI to set the default browser if necessary.
void ShowDefaultBrowserPrompt(Profile* profile);

}  // namespace browser

#endif  // CHROME_BROWSER_UI_DEFAULT_BROWSER_PROMPT_H_
