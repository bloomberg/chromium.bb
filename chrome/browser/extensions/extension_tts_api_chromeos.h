// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TTS_API_CHROMEOS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_TTS_API_CHROMEOS_H_
#pragma once

// This is called by AudioMixerAlsa to signal that it's safe to begin
// using TTS on ChromeOS - this must happen after AudioMixerAlsa has
// finished its initialization. NotificationService would be cleaner,
// but it's not available at the point when this code needs to be called.
void EnableChromeOsTts();

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_TTS_API_CHROMEOS_H_
