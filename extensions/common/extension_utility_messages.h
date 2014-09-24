// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, so no include guard.

#include <string>

#include "extensions/common/update_manifest.h"
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START ExtensionUtilityMsgStart

IPC_STRUCT_TRAITS_BEGIN(UpdateManifest::Result)
  IPC_STRUCT_TRAITS_MEMBER(extension_id)
  IPC_STRUCT_TRAITS_MEMBER(version)
  IPC_STRUCT_TRAITS_MEMBER(browser_min_version)
  IPC_STRUCT_TRAITS_MEMBER(package_hash)
  IPC_STRUCT_TRAITS_MEMBER(crx_url)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(UpdateManifest::Results)
  IPC_STRUCT_TRAITS_MEMBER(list)
  IPC_STRUCT_TRAITS_MEMBER(daystart_elapsed_seconds)
IPC_STRUCT_TRAITS_END()

//------------------------------------------------------------------------------
// Utility process messages:
// These are messages from the browser to the utility process.

// Tell the utility process to parse the given xml document.
IPC_MESSAGE_CONTROL1(ExtensionUtilityMsg_ParseUpdateManifest,
                     std::string /* xml document contents */)

//------------------------------------------------------------------------------
// Utility process host messages:
// These are messages from the utility process to the browser.

// Reply when the utility process has succeeded in parsing an update manifest
// xml document.
IPC_MESSAGE_CONTROL1(ExtensionUtilityHostMsg_ParseUpdateManifest_Succeeded,
                     UpdateManifest::Results /* updates */)

// Reply when an error occurred parsing the update manifest. |error_message|
// is a description of what went wrong suitable for logging.
IPC_MESSAGE_CONTROL1(ExtensionUtilityHostMsg_ParseUpdateManifest_Failed,
                     std::string /* error_message, if any */)
