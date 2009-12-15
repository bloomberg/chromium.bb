// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header is meant to be included in multiple passes, hence no traditional
// header guard. It is included by utility_messages_internal.h
// See ipc_message_macros.h for explanation of the macros and passes.

// This file needs to be included again, even though we're actually included
// from it via utility_messages.h.
#include "ipc/ipc_message_macros.h"

//------------------------------------------------------------------------------
// Utility process messages:
// These are messages from the browser to the utility process.
IPC_BEGIN_MESSAGES(Utility)

  // Tell the utility process to unpack the given extension file in its
  // directory and verify that it is valid.
  IPC_MESSAGE_CONTROL1(UtilityMsg_UnpackExtension,
                       FilePath /* extension_filename */)

  // Tell the utility process to parse the given JSON data and verify its
  // validity.
  IPC_MESSAGE_CONTROL1(UtilityMsg_UnpackWebResource,
                       std::string /* JSON data */)

  // Tell the utility process to parse the given xml document.
  IPC_MESSAGE_CONTROL1(UtilityMsg_ParseUpdateManifest,
                       std::string /* xml document contents */)

IPC_END_MESSAGES(Utility)

//------------------------------------------------------------------------------
// Utility process host messages:
// These are messages from the utility process to the browser.
IPC_BEGIN_MESSAGES(UtilityHost)

  // Reply when the utility process is done unpacking an extension.  |manifest|
  // is the parsed manifest.json file.  |catalogs| is the list of all parsed
  // message catalogs and relative paths to them.
  // The unpacker should also have written out a file containing decoded images
  // from the extension.  See ExtensionUnpacker for details.
  IPC_MESSAGE_CONTROL2(UtilityHostMsg_UnpackExtension_Succeeded,
                       DictionaryValue /* manifest */,
                       DictionaryValue /* catalogs */)

  // Reply when the utility process has failed while unpacking an extension.
  // |error_message| is a user-displayable explanation of what went wrong.
  IPC_MESSAGE_CONTROL1(UtilityHostMsg_UnpackExtension_Failed,
                       std::string /* error_message, if any */)

  // Reply when the utility process is done unpacking and parsing JSON data
  // from a web resource.
  IPC_MESSAGE_CONTROL1(UtilityHostMsg_UnpackWebResource_Succeeded,
                       DictionaryValue /* json data */)

  // Reply when the utility process has failed while unpacking and parsing a
  // web resource.  |error_message| is a user-readable explanation of what
  // went wrong.
  IPC_MESSAGE_CONTROL1(UtilityHostMsg_UnpackWebResource_Failed,
                       std::string /* error_message, if any */)

  // Reply when the utility process has succeeded in parsing an update manifest
  // xml document.
  IPC_MESSAGE_CONTROL1(UtilityHostMsg_ParseUpdateManifest_Succeeded,
                       std::vector<UpdateManifest::Result> /* updates */)

  // Reply when an error occured parsing the update manifest. |error_message|
  // is a description of what went wrong suitable for logging.
  IPC_MESSAGE_CONTROL1(UtilityHostMsg_ParseUpdateManifest_Failed,
                       std::string /* error_message, if any */)

IPC_END_MESSAGES(UtilityHost)
