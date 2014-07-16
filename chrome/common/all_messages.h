// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, hence no include guard.
// Inclusion of all message files present in chrome. Keep this file
// up-to-date when adding a new value to the IPCMessageStart enum in
// ipc/ipc_message_start.h to ensure the corresponding message file is
// included here. Message classes used exclusively outside of chrome
// should not be listed here and instead get an exemption in
// chrome/tools/ipclist/ipclist.cc.
#if !defined(OS_ANDROID)
#include "chrome/common/importer/profile_import_process_messages.h"
#endif

#include "chrome/common/common_message_generator.h"

#if !defined(DISABLE_NACL)
#include "components/nacl/common/nacl_messages.h"
#endif
