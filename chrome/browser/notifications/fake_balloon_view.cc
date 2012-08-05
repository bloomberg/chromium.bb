// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/fake_balloon_view.h"

FakeBalloonView::FakeBalloonView(Balloon* balloon)
    : balloon_(balloon) {
}

FakeBalloonView::~FakeBalloonView() {
}

void FakeBalloonView::Show(Balloon* balloon) {
}

void FakeBalloonView::Update() {
}

void FakeBalloonView::RepositionToBalloon() {
}

void FakeBalloonView::Close(bool by_user) {
  balloon_->OnClose(by_user);
}

gfx::Size FakeBalloonView::GetSize() const {
  return balloon_->content_size();
}

BalloonHost* FakeBalloonView::GetHost() const {
  return NULL;
}
