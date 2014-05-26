// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/user_selection_screen.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/login/screens/screen_observer.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"

namespace chromeos {

UserSelectionScreen::UserSelectionScreen() : handler_(NULL) {
}

UserSelectionScreen::~UserSelectionScreen() {
}

void UserSelectionScreen::SetHandler(LoginDisplayWebUIHandler* handler) {
  handler_ = handler;
}

void UserSelectionScreen::Init(const UserList& users) {
  users_ = users;
}

void UserSelectionScreen::OnBeforeUserRemoved(const std::string& username) {
  for (UserList::iterator it = users_.begin(); it != users_.end(); ++it) {
    if ((*it)->email() == username) {
      users_.erase(it);
      break;
    }
  }
}

void UserSelectionScreen::OnUserRemoved(const std::string& username) {
  if (!handler_)
    return;

  handler_->OnUserRemoved(username);
}

void UserSelectionScreen::OnUserImageChanged(const User& user) {
  if (!handler_)
    return;
  handler_->OnUserImageChanged(user);
  // TODO(antrim) : updateUserImage(user.email())
}

const UserList& UserSelectionScreen::GetUsers() const {
  return users_;
}

}  // namespace chromeos
