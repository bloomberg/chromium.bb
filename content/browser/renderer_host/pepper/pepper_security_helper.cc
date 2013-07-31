// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_security_helper.h"

#include "base/logging.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "ppapi/c/ppb_file_io.h"

namespace content {

bool CanOpenWithPepperFlags(int pp_open_flags, int child_id,
                            const base::FilePath& file) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  bool pp_read = !!(pp_open_flags & PP_FILEOPENFLAG_READ);
  bool pp_write = !!(pp_open_flags & PP_FILEOPENFLAG_WRITE);
  bool pp_create = !!(pp_open_flags & PP_FILEOPENFLAG_CREATE);
  bool pp_truncate = !!(pp_open_flags & PP_FILEOPENFLAG_TRUNCATE);
  bool pp_exclusive = !!(pp_open_flags & PP_FILEOPENFLAG_EXCLUSIVE);
  bool pp_append = !!(pp_open_flags & PP_FILEOPENFLAG_APPEND);

  if (pp_read && !policy->CanReadFile(child_id, file))
    return false;

  if (pp_write && !policy->CanWriteFile(child_id, file))
    return false;

  if (pp_append) {
    // Given ChildSecurityPolicyImpl's current definition of permissions,
    // APPEND is never supported.
    return false;
  }

  if (pp_truncate && !pp_write)
    return false;

  if (pp_create) {
    if (pp_exclusive) {
      return policy->CanCreateFile(child_id, file);
    } else {
      // Asks for too much, but this is the only grant that allows overwrite.
      return policy->CanCreateWriteFile(child_id, file);
    }
  } else if (pp_truncate) {
    return policy->CanCreateWriteFile(child_id, file);
  }

  return true;
}

}  // namespace content
