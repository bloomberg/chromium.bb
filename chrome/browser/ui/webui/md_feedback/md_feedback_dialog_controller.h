// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MD_FEEDBACK_MD_FEEDBACK_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEBUI_MD_FEEDBACK_MD_FEEDBACK_DIALOG_CONTROLLER_H_

#include "base/memory/singleton.h"
#include "content/public/browser/browser_context.h"

// Manages MD Feedback dialog creation, destruction, and showing a constrained
// web dialog with the feedback contents. This controller is implemented as a
// singleton.
class MdFeedbackDialogController {
 public:
  static MdFeedbackDialogController* GetInstance();

  // Show the feedback dialog. This WebUI dialog is not anchored to the browser
  // window but tied to a profile.
  void Show(content::BrowserContext* browser_context);

 private:
  friend struct base::DefaultSingletonTraits<MdFeedbackDialogController>;

  MdFeedbackDialogController();
  ~MdFeedbackDialogController() = default;

  DISALLOW_COPY_AND_ASSIGN(MdFeedbackDialogController);
};

#endif  // CHROME_BROWSER_UI_WEBUI_MD_FEEDBACK_MD_FEEDBACK_DIALOG_CONTROLLER_H_
