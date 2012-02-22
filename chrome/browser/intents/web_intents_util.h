// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTENTS_WEB_INTENTS_UTIL_H_
#define CHROME_BROWSER_INTENTS_WEB_INTENTS_UTIL_H_
#pragma once

class PrefService;

namespace web_intents {

// Registers the preferences related to Web Intents.
void RegisterUserPrefs(PrefService* user_prefs);

// Returns true if Web Intent is enabled due to various factors. |profile| is
// the current, active Profile.
bool IsWebIntentsEnabled();

}  // namespace web_intents

#endif  // CHROME_BROWSER_INTENTS_WEB_INTENTS_UTIL_H_
