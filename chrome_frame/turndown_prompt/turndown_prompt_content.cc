// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/turndown_prompt/turndown_prompt_content.h"

#include "base/logging.h"
#include "chrome_frame/ready_mode/internal/url_launcher.h"
#include "chrome_frame/turndown_prompt/turndown_prompt_window.h"

TurndownPromptContent::TurndownPromptContent(
    UrlLauncher* url_launcher,
    const base::Closure& uninstall_closure)
    : url_launcher_(url_launcher),
      uninstall_closure_(uninstall_closure) {
}

TurndownPromptContent::~TurndownPromptContent() {
  if (window_ != NULL && window_->IsWindow()) {
    // The window must discard its ContentFrame pointer at this time.
    window_->DestroyWindow();
    window_.reset();
  }
}

bool TurndownPromptContent::InstallInFrame(Frame* frame) {
  DCHECK(window_ == NULL);
  DCHECK(url_launcher_ != NULL);

  // Pass ownership of our url_launcher_ to the window.
  window_ = TurndownPromptWindow::CreateInstance(
      frame, url_launcher_.release(), uninstall_closure_);
  uninstall_closure_.Reset();

  return window_ != NULL;
}

void TurndownPromptContent::SetDimensions(const RECT& dimensions) {
  if (window_ != NULL && window_->IsWindow()) {
    window_->SetWindowPos(HWND_TOP, &dimensions,
                          ::IsRectEmpty(&dimensions) ? SWP_HIDEWINDOW :
                                                       SWP_SHOWWINDOW);
  }
}

bool GetDialogTemplateDimensions(TurndownPromptWindow* window,
                                 RECT* dimensions) {
  HRSRC resource = ::FindResource(_AtlBaseModule.GetResourceInstance(),
                                  MAKEINTRESOURCE(TurndownPromptWindow::IDD),
                                  RT_DIALOG);

  HGLOBAL handle = NULL;
  DLGTEMPLATE* dlgtemplate = NULL;
  _DialogSplitHelper::DLGTEMPLATEEX* dlgtemplateex = NULL;

  if (resource == NULL) {
    DPLOG(ERROR) << "Failed to find resource for TurndownPromptWindow::IDD";
    return false;
  }

  handle = ::LoadResource(_AtlBaseModule.GetResourceInstance(), resource);

  if (handle == NULL) {
    DPLOG(ERROR) << "Failed to load resource for TurndownPromptWindow::IDD";
    return false;
  }

  dlgtemplate = reinterpret_cast<DLGTEMPLATE*>(::LockResource(handle));
  if (dlgtemplate == NULL) {
    DPLOG(ERROR) << "Failed to lock resource for TurndownPromptWindow::IDD";
    return false;
  }

  if (!_DialogSplitHelper::IsDialogEx(dlgtemplate)) {
    DLOG(ERROR) << "Resource TurndownPromptWindow::IDD is not a DLGTEMPLATEEX";
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

size_t TurndownPromptContent::GetDesiredSize(size_t width, size_t height) {
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
