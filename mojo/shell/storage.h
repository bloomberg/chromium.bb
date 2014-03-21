// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_STORAGE_H_
#define MOJO_SHELL_STORAGE_H_

#include "base/files/file_path.h"

namespace mojo {
namespace shell {

// An object that represents the persistent storage used by the shell.
class Storage {
 public:
  Storage();
  ~Storage();

  base::FilePath profile_path() const {
    return profile_path_;
  }

 private:
  base::FilePath profile_path_;

  DISALLOW_COPY_AND_ASSIGN(Storage);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_STORAGE_H_
