// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEEDBACK_UI_H_
#define CHROME_BROWSER_UI_WEBUI_FEEDBACK_UI_H_

#include <string>

#include "base/basictypes.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace base {
class FilePath;
}

class FeedbackUI : public ui::WebDialogUI {
 public:
  explicit FeedbackUI(content::WebUI* web_ui);

#if defined(OS_CHROMEOS)
  static void GetMostRecentScreenshots(
      const base::FilePath& filepath,
      std::vector<std::string>* saved_screenshots,
      size_t max_saved);
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(FeedbackUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_FEEDBACK_UI_H_
