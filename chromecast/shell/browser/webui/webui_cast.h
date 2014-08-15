// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_SHELL_BROWSER_WEBUI_WEBUI_CAST_H_
#define CHROMECAST_SHELL_BROWSER_WEBUI_WEBUI_CAST_H_

namespace chromecast {
namespace shell {

// Initializes all WebUIs needed for the Chromecast shell. This should be
// implemented on a per-product basis.
void InitializeWebUI();

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_SHELL_BROWSER_UI_WEBUI_CAST_H_
