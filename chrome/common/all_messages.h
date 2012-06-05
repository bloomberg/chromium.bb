// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, hence no include guard.
// Inclusion of all message files present in chrome.  Keep this file
// up-to-date when adding a new value to enum IPCMessageStart in
// ipc/ipc_message_utils.h to include the corresponding message file.
// Messages classes used exclusively outside of chrome should instead get an
// exemption in chrome/tools/ipclist/ipclist.cc.
#include "chrome/browser/importer/profile_import_process_messages.h"
// We can't make common_message_generator.h include automation_messages, since
// otherwise the Chrome Frame binaries will link in a lot of unrelated chrome
// code. Chrome Frame should not be depending on the chrome target...
// See http:://crbug.com/101208 and http://crbug.com/101215
#include "chrome/common/automation_messages.h"
#include "chrome/common/common_message_generator.h"
#include "chrome/common/nacl_messages.h"
#include "content/common/content_message_generator.h"
#include "ppapi/proxy/pepper_file_messages.h"
#include "ppapi/proxy/ppapi_messages.h"
