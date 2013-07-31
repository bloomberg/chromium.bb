// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_SECURITY_HELPER_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_SECURITY_HELPER_H_

#include "base/files/file_path.h"
#include "content/common/content_export.h"

namespace content {

// Helper method that returns whether or not the child process is allowed to
// open the specified |file| with the specified |pp_open_flags|.
CONTENT_EXPORT bool CanOpenWithPepperFlags(int pp_open_flags,
                                           int child_id,
                                           const base::FilePath& file);

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_SECURITY_HELPER_H_
