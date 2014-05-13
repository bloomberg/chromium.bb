// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_linux.h"

#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_layout.h"
#include "ui/views/linux_ui/linux_ui.h"

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameViewLinux, public:

OpaqueBrowserFrameViewLinux::OpaqueBrowserFrameViewLinux(
    OpaqueBrowserFrameView* view,
    OpaqueBrowserFrameViewLayout* layout,
    Profile* profile)
    : view_(view),
      layout_(layout),
      theme_service_(ThemeServiceFactory::GetForProfile(profile)) {
  views::LinuxUI* ui = views::LinuxUI::instance();
  if (ui)
    ui->AddWindowButtonOrderObserver(this);
}

OpaqueBrowserFrameViewLinux::~OpaqueBrowserFrameViewLinux() {
  views::LinuxUI* ui = views::LinuxUI::instance();
  if (ui)
    ui->RemoveWindowButtonOrderObserver(this);
}

bool OpaqueBrowserFrameViewLinux::IsUsingSystemTheme() {
  // On X11, this does the correct thing. On Windows, UsingSystemTheme() will
  // return true when using the default blue theme too.
  return theme_service_->UsingSystemTheme();
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameViewLinux,
//     views::WindowButtonOrderObserver implementation:

void OpaqueBrowserFrameViewLinux::OnWindowButtonOrderingChange(
    const std::vector<views::FrameButton>& leading_buttons,
    const std::vector<views::FrameButton>& trailing_buttons) {
  layout_->SetButtonOrdering(leading_buttons, trailing_buttons);

  // We can receive OnWindowButtonOrderingChange events before we've been added
  // to a Widget. We need a Widget because layout crashes due to dependencies
  // on a ui::ThemeProvider().
  if (view_->GetWidget()) {
    view_->Layout();
    view_->SchedulePaint();
  }
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameViewObserver:

// static
OpaqueBrowserFrameViewPlatformSpecific*
OpaqueBrowserFrameViewPlatformSpecific::Create(
      OpaqueBrowserFrameView* view,
      OpaqueBrowserFrameViewLayout* layout,
      Profile* profile) {
  return new OpaqueBrowserFrameViewLinux(view, layout, profile);
}
