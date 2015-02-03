// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/mock_update_screen.h"

using ::testing::AtLeast;
using ::testing::_;

namespace chromeos {

MockUpdateScreen::MockUpdateScreen(BaseScreenDelegate* base_screen_delegate,
                                   UpdateView* view)
    : UpdateScreen(base_screen_delegate, view, NULL) {
}

MockUpdateScreen::~MockUpdateScreen() {
}

MockUpdateView::MockUpdateView() : model_(nullptr) {
  EXPECT_CALL(*this, MockBind(_)).Times(AtLeast(1));
}

MockUpdateView::~MockUpdateView() {
  if (model_)
    model_->OnViewDestroyed(this);
}

void MockUpdateView::Bind(UpdateModel& model) {
  model_ = &model;
  MockBind(model);
}

void MockUpdateView::Unbind() {
  model_ = nullptr;
  MockUnbind();
}

}  // namespace chromeos
