// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/balloon.h"

#include "base/logging.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/notifications/balloon_collection.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

#if !defined(OS_WIN) && !defined(USE_AURA)
// static
int BalloonView::GetHorizontalMargin() {
  // TODO: implement for linux (non-aura) and mac.
  return 0;
}
#endif

Balloon::Balloon(const Notification& notification, Profile* profile,
                 BalloonCollection* collection)
    : profile_(profile),
      notification_(new Notification(notification)),
      collection_(collection) {
}

Balloon::~Balloon() {
}

void Balloon::SetPosition(const gfx::Point& upper_left, bool reposition) {
  position_ = upper_left;
  if (reposition && balloon_view_.get())
    balloon_view_->RepositionToBalloon();
}

void Balloon::ResizeDueToAutoResize(const gfx::Size& size) {
  collection_->ResizeBalloon(this, size);
}

void Balloon::set_view(BalloonView* balloon_view) {
  balloon_view_.reset(balloon_view);
}

void Balloon::Show() {
  notification_->Display();
  if (balloon_view_.get()) {
    balloon_view_->Show(this);
    balloon_view_->RepositionToBalloon();
  }
}

void Balloon::Update(const Notification& notification) {
  notification_->Close(false);
  notification_.reset(new Notification(notification));
  notification_->Display();
  if (balloon_view_.get()) {
    balloon_view_->Update();
  }
}

void Balloon::OnClick() {
  notification_->Click();
}

void Balloon::OnClose(bool by_user) {
  notification_->Close(by_user);
  collection_->OnBalloonClosed(this);
}

void Balloon::OnButtonClick(int button_index) {
  notification_->ButtonClick(button_index);
}

void Balloon::CloseByScript() {
  // A user-initiated close begins with the view and then closes this object;
  // we simulate that with a script-initiated close but pass |by_user|=false.
  DCHECK(balloon_view_.get());
  balloon_view_->Close(false);
}

std::string Balloon::GetExtensionId() {
  const ExtensionURLInfo url(notification().origin_url());
  const ExtensionService* service = profile()->GetExtensionService();
  const extensions::Extension* extension =
      service->extensions()->GetExtensionOrAppByURL(url);
  return extension ? extension->id() : std::string();
}
