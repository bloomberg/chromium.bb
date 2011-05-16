// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_FILE_SYSTEM_URL_REQUEST_JOB_FACTORY_H_
#define CHROME_BROWSER_NET_FILE_SYSTEM_URL_REQUEST_JOB_FACTORY_H_

#include "net/url_request/url_request_job_factory.h"

namespace fileapi {
class FileSystemContext;
}  // namespace fileapi

// |context|'s lifetime should exceed the lifetime of the ProtocolHandler.
// Currently, this is only used by ProfileIOData which owns |context| and the
// ProtocolHandler.
net::URLRequestJobFactory::ProtocolHandler*
CreateFileSystemProtocolHandler(fileapi::FileSystemContext* context);

#endif  // CHROME_BROWSER_NET_FILE_SYSTEM_URL_REQUEST_JOB_FACTORY_H_
