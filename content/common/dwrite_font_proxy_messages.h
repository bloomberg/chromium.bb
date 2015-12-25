// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START DWriteFontProxyMsgStart

#ifndef CONTENT_COMMON_DWRITE_FONT_PROXY_MESSAGES_H_
#define CONTENT_COMMON_DWRITE_FONT_PROXY_MESSAGES_H_

// The macros can't handle a complex template declaration, so we typedef it.
typedef std::pair<base::string16, base::string16> DWriteStringPair;

#endif  // CONTENT_COMMON_DWRITE_FONT_PROXY_MESSAGES_H_

// Locates the index of the specified font family within the system collection.
IPC_SYNC_MESSAGE_CONTROL1_1(DWriteFontProxyMsg_FindFamily,
                            base::string16 /* family_name */,
                            uint32_t /* out index */)

// Returns the number of font families in the system collection.
IPC_SYNC_MESSAGE_CONTROL0_1(DWriteFontProxyMsg_GetFamilyCount,
                            uint32_t /* out count */)

// Returns the list of locale and family name pairs for the font family at the
// specified index.
IPC_SYNC_MESSAGE_CONTROL1_1(
    DWriteFontProxyMsg_GetFamilyNames,
    uint32_t /* family_index */,
    std::vector<DWriteStringPair> /* out family_names */)

// Returns the list of font file paths in the system font directory that contain
// font data for the font family at the specified index.
IPC_SYNC_MESSAGE_CONTROL1_1(DWriteFontProxyMsg_GetFontFiles,
                            uint32_t /* family_index */,
                            std::vector<base::string16> /* out file_paths */)
