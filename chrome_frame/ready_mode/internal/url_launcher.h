// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_READY_MODE_INTERNAL_URL_LAUNCHER_H_
#define CHROME_FRAME_READY_MODE_INTERNAL_URL_LAUNCHER_H_
#pragma once

// Implements the launching of a new browser window/tab. The
// ReadyPromptContent invokes this, in response to user action, to display
// documentation about Ready Mode.
class UrlLauncher {
 public:
  virtual ~UrlLauncher() {}

  // Displays the specified URL in a new window or tab.
  virtual void LaunchUrl(const std::wstring& url) = 0;
};  // class UrlHelper

#endif  // CHROME_FRAME_READY_MODE_INTERNAL_URL_LAUNCHER_H_
