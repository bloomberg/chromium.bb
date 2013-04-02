// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_CONTENTS_MODAL_DIALOG_HOST_H_
#define CHROME_BROWSER_UI_WEB_CONTENTS_MODAL_DIALOG_HOST_H_

#include "ui/gfx/point.h"
#include "ui/gfx/size.h"

// Observer to be implemented to update web contents modal dialogs when the host
// indicates their position needs to be changed.
class WebContentsModalDialogHostObserver {
 public:
  virtual ~WebContentsModalDialogHostObserver();

  virtual void OnPositionRequiresUpdate() = 0;

 protected:
  WebContentsModalDialogHostObserver();

 private:
  DISALLOW_COPY_AND_ASSIGN(WebContentsModalDialogHostObserver);
};

// Interface for supporting positioning of web contents modal dialogs over a
// window/widget.
class WebContentsModalDialogHost {
 public:
  virtual ~WebContentsModalDialogHost();

  virtual gfx::Point GetDialogPosition(const gfx::Size& size) = 0;

  // Add/remove observer.
  virtual void AddObserver(WebContentsModalDialogHostObserver* observer) = 0;
  virtual void RemoveObserver(WebContentsModalDialogHostObserver* observer) = 0;

 protected:
  WebContentsModalDialogHost();

 private:
  DISALLOW_COPY_AND_ASSIGN(WebContentsModalDialogHost);
};

#endif  // CHROME_BROWSER_UI_WEB_CONTENTS_MODAL_DIALOG_HOST_H_
