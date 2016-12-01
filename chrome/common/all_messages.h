// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, hence no include guard.
// Inclusion of all message files present in chrome. Keep this file
// up to date when adding a new value to the IPCMessageStart enum in
// ipc/ipc_message_start.h to ensure the corresponding message file is
// included here. Message classes used exclusively outside of chrome
// should not be listed here and instead get an exemption in
// chrome/tools/ipclist/ipclist.cc.

#include "build/build_config.h"
#include "printing/features/features.h"

#include "chrome/common/common_message_generator.h"

#if BUILDFLAG(ENABLE_PRINTING)
// TODO(dgn) remove from here when all the code using these messages is removed
// from /chrome. (crbug.com/311308, crbug.com/450822)
#include "components/printing/common/print_messages.h"  // nogncheck
#endif

#if !defined(DISABLE_NACL)
#include "components/nacl/common/nacl_messages.h"
#endif
