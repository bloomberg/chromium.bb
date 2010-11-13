// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mime_registry_dispatcher.h"

#include "chrome/browser/browser_thread.h"
#include "chrome/common/render_messages.h"
#include "net/base/mime_util.h"

MimeRegistryDispatcher::MimeRegistryDispatcher(IPC::Message::Sender* sender)
    : message_sender_(sender) {
  DCHECK(message_sender_);
}

MimeRegistryDispatcher::~MimeRegistryDispatcher() {
}

void MimeRegistryDispatcher::Shutdown() {
  message_sender_ = NULL;
}

bool MimeRegistryDispatcher::OnMessageReceived(const IPC::Message& message) {
  // On Windows MIME registry requests may access the Windows Registry so
  // they need to run on the FILE thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    // Return false if the message is not for us.
    if (message.type() != ViewHostMsg_GetMimeTypeFromExtension::ID &&
        message.type() != ViewHostMsg_GetMimeTypeFromFile::ID &&
        message.type() != ViewHostMsg_GetPreferredExtensionForMimeType::ID)
     return false;

    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(
            this, &MimeRegistryDispatcher::OnMessageReceived, message));
    return true;
  }
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MimeRegistryDispatcher, message)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetMimeTypeFromExtension,
                                    OnGetMimeTypeFromExtension)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetMimeTypeFromFile,
                                    OnGetMimeTypeFromFile)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        ViewHostMsg_GetPreferredExtensionForMimeType,
        OnGetPreferredExtensionForMimeType)
    IPC_MESSAGE_UNHANDLED((handled = false))
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MimeRegistryDispatcher::OnGetMimeTypeFromExtension(
    const FilePath::StringType& ext, IPC::Message* reply_msg) {
  std::string mime_type;
  net::GetMimeTypeFromExtension(ext, &mime_type);
  ViewHostMsg_GetMimeTypeFromExtension::WriteReplyParams(reply_msg, mime_type);
  Send(reply_msg);
}

void MimeRegistryDispatcher::OnGetMimeTypeFromFile(
    const FilePath& file_path, IPC::Message* reply_msg) {
  std::string mime_type;
  net::GetMimeTypeFromFile(file_path, &mime_type);
  ViewHostMsg_GetMimeTypeFromFile::WriteReplyParams(reply_msg, mime_type);
  Send(reply_msg);
}

void MimeRegistryDispatcher::OnGetPreferredExtensionForMimeType(
    const std::string& mime_type, IPC::Message* reply_msg) {
  FilePath::StringType ext;
  net::GetPreferredExtensionForMimeType(mime_type, &ext);
  ViewHostMsg_GetPreferredExtensionForMimeType::WriteReplyParams(
      reply_msg, ext);
  Send(reply_msg);
}

void MimeRegistryDispatcher::Send(IPC::Message* message) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    if (!BrowserThread::PostTask(
            BrowserThread::IO, FROM_HERE, NewRunnableMethod(
                this, &MimeRegistryDispatcher::Send, message))) {
      // The IO thread is dead.
      delete message;
    }
    return;
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (message_sender_)
    message_sender_->Send(message);
  else
    delete message;
}
