// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_OPTIONS_HANDLERS_HELPER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_OPTIONS_HANDLERS_HELPER_H_

#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/host_desktop.h"

namespace content {
class WebUI;
}

namespace options {
namespace helper {

void OpenNewWindowForProfile(Profile* profile, Profile::CreateStatus status);

// Deletes the profile at the given |file_path|.
void DeleteProfileAtPath(base::FilePath file_path, content::WebUI* web_ui);

}  // namespace helper
}  // namespace options



#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_OPTIONS_HANDLERS_HELPER_H_
