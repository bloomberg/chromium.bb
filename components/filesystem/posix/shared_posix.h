// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Shared implementation of things common between |DirectoryImpl| and
// |FileImpl|.

#ifndef COMPONENTS_FILESYSTEM_POSIX_SHARED_POSIX_H_
#define COMPONENTS_FILESYSTEM_POSIX_SHARED_POSIX_H_

#include "components/filesystem/public/interfaces/types.mojom.h"
#include "mojo/public/cpp/bindings/callback.h"

namespace filesystem {

// Stats the given FD (which must be valid), calling |callback| appropriately.
// The type in the |FileInformation| given to the callback will be assigned from
// |type|.
using StatFDCallback = mojo::Callback<void(Error, FileInformationPtr)>;
void StatFD(int fd, FileType type, const StatFDCallback& callback);

// Touches the given FD (which must be valid), calling |callback| appropriately.
using TouchFDCallback = mojo::Callback<void(Error)>;
void TouchFD(int fd,
             TimespecOrNowPtr atime,
             TimespecOrNowPtr mtime,
             const TouchFDCallback& callback);

}  // namespace filesystem

#endif  // COMPONENTS_FILESYSTEM_POSIX_SHARED_POSIX_H_
