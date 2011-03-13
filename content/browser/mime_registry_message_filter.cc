// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mime_registry_message_filter.h"

#include "content/common/mime_registry_messages.h"
#include "net/base/mime_util.h"

MimeRegistryMessageFilter::MimeRegistryMessageFilter() {
}

MimeRegistryMessageFilter::~MimeRegistryMessageFilter() {
}

void MimeRegistryMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  if (IPC_MESSAGE_CLASS(message) == MimeRegistryMsgStart)
    *thread = BrowserThread::FILE;
}

bool MimeRegistryMessageFilter::OnMessageReceived(const IPC::Message& message,
                                                  bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(MimeRegistryMessageFilter, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(MimeRegistryMsg_GetMimeTypeFromExtension,
                        OnGetMimeTypeFromExtension)
    IPC_MESSAGE_HANDLER(MimeRegistryMsg_GetMimeTypeFromFile,
                        OnGetMimeTypeFromFile)
    IPC_MESSAGE_HANDLER(MimeRegistryMsg_GetPreferredExtensionForMimeType,
                        OnGetPreferredExtensionForMimeType)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MimeRegistryMessageFilter::OnGetMimeTypeFromExtension(
    const FilePath::StringType& ext, std::string* mime_type) {
  net::GetMimeTypeFromExtension(ext, mime_type);
}

void MimeRegistryMessageFilter::OnGetMimeTypeFromFile(
    const FilePath& file_path, std::string* mime_type) {
  net::GetMimeTypeFromFile(file_path, mime_type);
}

void MimeRegistryMessageFilter::OnGetPreferredExtensionForMimeType(
    const std::string& mime_type, FilePath::StringType* extension) {
  net::GetPreferredExtensionForMimeType(mime_type, extension);
}
