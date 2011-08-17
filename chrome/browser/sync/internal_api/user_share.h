// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_INTERNAL_API_USER_SHARE_H_
#define CHROME_BROWSER_SYNC_INTERNAL_API_USER_SHARE_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"

namespace syncable {
class DirectoryManager;
}

namespace sync_api {

// A UserShare encapsulates the syncable pieces that represent an authenticated
// user and their data (share).
// This encompasses all pieces required to build transaction objects on the
// syncable share.
struct UserShare {
  UserShare();
  ~UserShare();

  // The DirectoryManager itself, which is the parent of Transactions and can
  // be shared across multiple threads (unlike Directory).
  scoped_ptr<syncable::DirectoryManager> dir_manager;

  // The username of the sync user.
  std::string name;
};

}

#endif  // CHROME_BROWSER_SYNC_INTERNAL_API_USER_SHARE_H_
