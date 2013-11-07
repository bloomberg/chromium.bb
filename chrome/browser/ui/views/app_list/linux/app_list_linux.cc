// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_list/linux/app_list_linux.h"

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"

AppListLinux::AppListLinux(app_list::AppListView* view,
                           const base::Closure& on_should_dismiss)
    : view_(view),
      window_icon_updated_(false),
      on_should_dismiss_(on_should_dismiss) {
  view_->AddObserver(this);
}

AppListLinux::~AppListLinux() {
  view_->RemoveObserver(this);
}

void AppListLinux::Show() {
  view_->GetWidget()->Show();
  if (!window_icon_updated_) {
    view_->GetWidget()->GetTopLevelWidget()->UpdateWindowIcon();
    window_icon_updated_ = true;
  }
  view_->GetWidget()->Activate();
}

void AppListLinux::Hide() {
  view_->GetWidget()->Hide();
}

void AppListLinux::MoveNearCursor() {
  view_->SetBubbleArrow(views::BubbleBorder::FLOAT);
  // Anchor to the top-left corner of the screen.
  // TODO(mgiuca): Try to anchor near the taskbar and/or mouse cursor. This will
  // depend upon the user's window manager.
  gfx::Size view_size = view_->GetPreferredSize();
  view_->SetAnchorPoint(
      gfx::Point(view_size.width() / 2, view_size.height() / 2));
}

bool AppListLinux::IsVisible() {
  return view_->GetWidget()->IsVisible();
}

void AppListLinux::Prerender() {
  view_->Prerender();
}

void AppListLinux::ReactivateOnNextFocusLoss() {
  // This is only used on Windows 8, so we ignore it on Linux.
}

gfx::NativeWindow AppListLinux::GetWindow() {
  return view_->GetWidget()->GetNativeWindow();
}

void AppListLinux::SetProfile(Profile* profile) {
  view_->SetProfileByPath(profile->GetPath());
}

void AppListLinux::OnActivationChanged(
    views::Widget* /*widget*/, bool active) {
  if (active)
    return;

  // Call |on_should_dismiss_| asynchronously. This must be done asynchronously
  // or our caller will crash, as it expects the app list to remain alive.
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, on_should_dismiss_);
}
