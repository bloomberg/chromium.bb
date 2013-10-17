// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_MODAL_TEST_WEB_CONTENTS_MODAL_DIALOG_HOST_H_
#define COMPONENTS_WEB_MODAL_TEST_WEB_CONTENTS_MODAL_DIALOG_HOST_H_

#include "components/web_modal/web_contents_modal_dialog_host.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

namespace web_modal {

class TestWebContentsModalDialogHost : public WebContentsModalDialogHost {
 public:
  explicit TestWebContentsModalDialogHost(gfx::NativeView host_view);
  virtual ~TestWebContentsModalDialogHost();

  // WebContentsModalDialogHost:
  virtual gfx::Size GetMaximumDialogSize() OVERRIDE;
  virtual gfx::NativeView GetHostView() const OVERRIDE;
  virtual gfx::Point GetDialogPosition(const gfx::Size& size) OVERRIDE;
  virtual void AddObserver(ModalDialogHostObserver* observer) OVERRIDE;
  virtual void RemoveObserver(ModalDialogHostObserver* observer) OVERRIDE;

  void set_max_dialog_size(const gfx::Size& max_dialog_size) {
    max_dialog_size_ = max_dialog_size;
  }

 private:
  gfx::NativeView host_view_;
  gfx::Size max_dialog_size_;

  DISALLOW_COPY_AND_ASSIGN(TestWebContentsModalDialogHost);
};

}  // namespace web_modal

#endif  // COMPONENTS_WEB_MODAL_TEST_WEB_CONTENTS_MODAL_DIALOG_HOST_H_
