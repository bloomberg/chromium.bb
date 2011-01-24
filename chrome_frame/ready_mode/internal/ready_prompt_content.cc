// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/ready_mode/internal/ready_prompt_content.h"

#include "base/logging.h"
#include "chrome_frame/ready_mode/internal/ready_mode_state.h"
#include "chrome_frame/ready_mode/internal/ready_prompt_window.h"
#include "chrome_frame/ready_mode/internal/url_launcher.h"

ReadyPromptContent::ReadyPromptContent(ReadyModeState* ready_mode_state,
                                       UrlLauncher* url_launcher)
    : ready_mode_state_(ready_mode_state),
      url_launcher_(url_launcher) {
}

ReadyPromptContent::~ReadyPromptContent() {
  if (window_ != NULL && window_->IsWindow()) {
    // The window must discard its ContentFrame pointer at this time.
    window_->DestroyWindow();
    window_.reset();
  }
}

bool ReadyPromptContent::InstallInFrame(Frame* frame) {
  DCHECK(window_ == NULL);
  DCHECK(ready_mode_state_ != NULL);
  DCHECK(url_launcher_ != NULL);

  // Pass ownership of our ready_mode_state_ and url_launcher_ to the window.
  window_ = ReadyPromptWindow::CreateInstance(
      frame, ready_mode_state_.release(), url_launcher_.release());

  return window_ != NULL;
}

void ReadyPromptContent::SetDimensions(const RECT& dimensions) {
  if (window_ != NULL && window_->IsWindow()) {
    window_->SetWindowPos(HWND_TOP, &dimensions,
                          ::IsRectEmpty(&dimensions) ? SWP_HIDEWINDOW :
                                                       SWP_SHOWWINDOW);
  }
}

bool GetDialogTemplateDimensions(ReadyPromptWindow* window, RECT* dimensions) {
  HRSRC resource = ::FindResource(_AtlBaseModule.GetResourceInstance(),
                                  MAKEINTRESOURCE(ReadyPromptWindow::IDD),
                                  RT_DIALOG);

  HGLOBAL handle = NULL;
  DLGTEMPLATE* dlgtemplate = NULL;
  _DialogSplitHelper::DLGTEMPLATEEX* dlgtemplateex = NULL;

  if (resource == NULL) {
    DPLOG(ERROR) << "Failed to find resource for ReadyPromptWindow::IDD";
    return false;
  }

  handle = ::LoadResource(_AtlBaseModule.GetResourceInstance(), resource);

  if (handle == NULL) {
    DPLOG(ERROR) << "Failed to load resource for ReadyPromptWindow::IDD";
    return false;
  }

  dlgtemplate = reinterpret_cast<DLGTEMPLATE*>(::LockResource(handle));
  if (dlgtemplate == NULL) {
    DPLOG(ERROR) << "Failed to lock resource for ReadyPromptWindow::IDD";
    return false;
  }

  if (!_DialogSplitHelper::IsDialogEx(dlgtemplate)) {
    DLOG(ERROR) << "Resource ReadyPromptWindow::IDD is not a DLGTEMPLATEEX";
    return false;
  }

  dlgtemplateex =
      reinterpret_cast<_DialogSplitHelper::DLGTEMPLATEEX*>(dlgtemplate);

  RECT dlgdimensions = {0, 0, dlgtemplateex->cx, dlgtemplateex->cy};
  if (!window->MapDialogRect(&dlgdimensions)) {
    DPLOG(ERROR) << "Failure in MapDialogRect";
    return false;
  }

  *dimensions = dlgdimensions;
  return true;
}

size_t ReadyPromptContent::GetDesiredSize(size_t width, size_t height) {
  DCHECK(window_ != NULL && window_->IsWindow());

  if (window_ == NULL || !window_->IsWindow()) {
    return 0;
  }
  RECT dialog_dimensions = {0, 0, 0, 0};

  if (GetDialogTemplateDimensions(window_.get(), &dialog_dimensions))
    return width == 0 ? dialog_dimensions.right : dialog_dimensions.bottom;
  else
    return width == 0 ? 0 : 39;
}
