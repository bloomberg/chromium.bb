// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_TOOLS_PACKAGE_MANAGER_UNPACKER_H_
#define MOJO_TOOLS_PACKAGE_MANAGER_UNPACKER_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {

// Unzips a package into a temporary folder. The temporary folder will be
// deleted when the object is destroyed.
//
// In the future, this class would probably manage doing the unzip operation on
// a background thread.
class Unpacker {
 public:
  Unpacker();
  ~Unpacker();

  // Actually does the unpacking, returns true on success.
  bool Unpack(const base::FilePath& zip_file);

  // The root directory where the package has been unpacked.
  const base::FilePath& dir() const { return dir_; }

 private:
  base::FilePath zip_file_;

  base::FilePath dir_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(Unpacker);
};

}  // namespace mojo

#endif  // MOJO_TOOLS_PACKAGE_MANAGER_UNPACKER_H_
