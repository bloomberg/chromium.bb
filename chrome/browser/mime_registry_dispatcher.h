// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MIME_REGISTRY_DISPATCHER_H_
#define CHROME_BROWSER_MIME_REGISTRY_DISPATCHER_H_

#include "base/file_path.h"
#include "base/ref_counted.h"
#include "ipc/ipc_message.h"

class MimeRegistryDispatcher
    : public base::RefCountedThreadSafe<MimeRegistryDispatcher> {
 public:
  explicit MimeRegistryDispatcher(IPC::Message::Sender* sender);
  void Shutdown();
  bool OnMessageReceived(const IPC::Message& message);
  void Send(IPC::Message* message);

 private:
  friend class base::RefCountedThreadSafe<MimeRegistryDispatcher>;
  ~MimeRegistryDispatcher();

  void OnGetMimeTypeFromExtension(const FilePath::StringType& ext,
                                  IPC::Message* reply);
  void OnGetMimeTypeFromFile(const FilePath& file_path,
                             IPC::Message* reply);
  void OnGetPreferredExtensionForMimeType(const std::string& mime_type,
                                          IPC::Message* reply);

  // The sender to be used for sending out IPC messages.
  IPC::Message::Sender* message_sender_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(MimeRegistryDispatcher);
};

#endif  // CHROME_BROWSER_MIME_REGISTRY_DISPATCHER_H_
