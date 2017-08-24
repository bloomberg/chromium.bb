// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_ZUCCHINI_ZUCCHINI_INTEGRATION_H_
#define CHROME_INSTALLER_ZUCCHINI_ZUCCHINI_INTEGRATION_H_

#include "base/files/file_path.h"
#include "chrome/installer/zucchini/zucchini.h"

namespace zucchini {

// Applies the patch in |patch_path| to the bytes in |old_path| and writes the
// result to |new_path|. Since this uses memory mapped files, crashes are
// expected in case of I/O errors. |new_path| is kept iff returned code is
// kStatusSuccess, and is deleted otherwise.
status::Code Apply(const base::FilePath& old_path,
                   const base::FilePath& patch_path,
                   const base::FilePath& new_path);

}  // namespace zucchini

#endif  // CHROME_INSTALLER_ZUCCHINI_ZUCCHINI_INTEGRATION_H_
