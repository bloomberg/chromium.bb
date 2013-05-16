// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_MODAL_WEB_CONTENTS_MODAL_DIALOG_HOST_H_
#define COMPONENTS_WEB_MODAL_WEB_CONTENTS_MODAL_DIALOG_HOST_H_

#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"
#include "ui/gfx/size.h"

namespace web_modal {

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

  // Returns the view against which the dialog is positioned and parented.
  virtual gfx::NativeView GetHostView() const = 0;
  // Gets the position for the dialog in coordinates relative to the host
  // view.
  virtual gfx::Point GetDialogPosition(const gfx::Size& size) = 0;

  // Add/remove observer.
  virtual void AddObserver(WebContentsModalDialogHostObserver* observer) = 0;
  virtual void RemoveObserver(WebContentsModalDialogHostObserver* observer) = 0;

 protected:
  WebContentsModalDialogHost();

 private:
  DISALLOW_COPY_AND_ASSIGN(WebContentsModalDialogHost);
};

}  // namespace web_modal

#endif  // COMPONENTS_WEB_MODAL_WEB_CONTENTS_MODAL_DIALOG_HOST_H_
