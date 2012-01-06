// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROME_WEB_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROME_WEB_UI_H_
#pragma once

namespace chrome_web_ui {

// IsMoreWebUI returns a command line flag that tracks whether to use
// available WebUI implementations of native dialogs.
bool IsMoreWebUI();

// Override the argument setting for more WebUI. If true this enables more
// WebUI.
void OverrideMoreWebUI(bool use_more_webui);

}  // namespace chrome_web_ui

#endif  // CHROME_BROWSER_UI_WEBUI_CHROME_WEB_UI_H_
