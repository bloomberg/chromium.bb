// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_UNLOAD_CONTROLLER_WEB_CONTENTS_DELEGATE_H_
#define CHROME_BROWSER_UI_UNLOAD_CONTROLLER_WEB_CONTENTS_DELEGATE_H_

#include "content/public/browser/web_contents_delegate.h"

// UnloadControllerWebContentsDelegate will delete web contents when they are
// closed. It's used by UnloadController when beforeunload event is skipped so
// that the existing dialogs will be closed as well. It's used by
// FastUnloadController regardless.
class UnloadControllerWebContentsDelegate
    : public content::WebContentsDelegate {
 public:
  UnloadControllerWebContentsDelegate();
  ~UnloadControllerWebContentsDelegate() override;

  // WebContentsDelegate implementation.
  bool ShouldSuppressDialogs(content::WebContents* source) override;
  void CloseContents(content::WebContents* source) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UnloadControllerWebContentsDelegate);
};

#endif  // CHROME_BROWSER_UI_UNLOAD_CONTROLLER_WEB_CONTENTS_DELEGATE_H_
