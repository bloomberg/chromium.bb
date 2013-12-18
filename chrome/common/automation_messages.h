// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, no traditional include guard.
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "chrome/common/automation_constants.h"
#include "chrome/common/common_param_traits.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/page_type.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "ui/gfx/rect.h"
#include "url/gurl.h"

IPC_ENUM_TRAITS(AutomationMsg_NavigationResponseValues)
IPC_ENUM_TRAITS(content::PageType)

IPC_STRUCT_BEGIN(AutomationMsg_Find_Params)
  // The word(s) to find on the page.
  IPC_STRUCT_MEMBER(base::string16, search_string)

  // Whether to search forward or backward within the page.
  IPC_STRUCT_MEMBER(bool, forward)

  // Whether search should be Case sensitive.
  IPC_STRUCT_MEMBER(bool, match_case)

  // Whether this operation is first request (Find) or a follow-up (FindNext).
  IPC_STRUCT_MEMBER(bool, find_next)
IPC_STRUCT_END()

// Keep this internal message file unchanged to preserve line numbering
// (and hence the dubious __LINE__-based message numberings) across versions.
#include "chrome/common/automation_messages_internal.h"
