// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_ABOUT_NETWORK_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_ABOUT_NETWORK_H_

#include <string>

namespace chromeos {
namespace about_ui {

// Returns the HTML for chrome://network for the chromeos networks.
// |query| contains any text after 'chrome://network/', used to indicate the
// refresh interval.
std::string AboutNetwork(const std::string& query);

}  // namespace about_ui
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_ABOUT_NETWORK_H_
