// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SYSTEM_INFO_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SYSTEM_INFO_HANDLER_H_

#include "content/browser/devtools/protocol/devtools_protocol_handler.h"

namespace content {
namespace devtools {
namespace system_info {

class SystemInfoHandler {
 public:
  typedef DevToolsProtocolClient::Response Response;

  SystemInfoHandler();
  virtual ~SystemInfoHandler();

  Response GetInfo(scoped_refptr<devtools::system_info::GPUInfo>* gpu,
                   std::string* model_name,
                   std::string* model_version);

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemInfoHandler);
};

}  // namespace system_info
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SYSTEM_INFO_HANDLER_H_
