// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/models/user_board_model.h"

namespace {
const char kUserBoardScreenName[] = "userBoard";
}

namespace chromeos {

UserBoardModel::UserBoardModel() : BaseScreen(nullptr) {}

UserBoardModel::~UserBoardModel() {}

std::string UserBoardModel::GetName() const {
  return kUserBoardScreenName;
}

}  // namespace chromeos
