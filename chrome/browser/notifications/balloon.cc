// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/balloon.h"

#include "base/gfx/rect.h"
#include "base/logging.h"
#include "chrome/browser/notifications/balloon_collection.h"
#include "chrome/browser/renderer_host/site_instance.h"

Balloon::Balloon(const Notification& notification, Profile* profile,
                 BalloonCloseListener* listener)
    : profile_(profile),
      notification_(notification),
      close_listener_(listener) {
}

Balloon::~Balloon() {
}

void Balloon::SetPosition(const gfx::Point& upper_left, bool reposition) {
  position_ = upper_left;
  if (reposition && balloon_view_.get())
    balloon_view_->RepositionToBalloon();
}

void Balloon::set_view(BalloonView* balloon_view) {
  balloon_view_.reset(balloon_view);
}

void Balloon::Show() {
  notification_.Display();
  if (balloon_view_.get()) {
    balloon_view_->Show(this);
  }
}

void Balloon::Close(bool by_user) {
  notification_.Close(by_user);
  if (close_listener_)
    close_listener_->OnBalloonClosed(this);
}
