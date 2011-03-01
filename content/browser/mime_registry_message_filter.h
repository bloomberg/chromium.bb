// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MIME_REGISTRY_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_MIME_REGISTRY_MESSAGE_FILTER_H_

#include "base/file_path.h"
#include "content/browser/browser_message_filter.h"

class MimeRegistryMessageFilter : public BrowserMessageFilter {
 public:
  MimeRegistryMessageFilter();

  virtual void OverrideThreadForMessage(const IPC::Message& message,
                                        BrowserThread::ID* thread);
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok);

 private:
  ~MimeRegistryMessageFilter();

  void OnGetMimeTypeFromExtension(const FilePath::StringType& ext,
                                  std::string* mime_type);
  void OnGetMimeTypeFromFile(const FilePath& file_path,
                             std::string* mime_type);
  void OnGetPreferredExtensionForMimeType(const std::string& mime_type,
                                          FilePath::StringType* extension);
};

#endif  // CONTENT_BROWSER_MIME_REGISTRY_MESSAGE_FILTER_H_
