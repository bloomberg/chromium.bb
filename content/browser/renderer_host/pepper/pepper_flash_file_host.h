// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_FLASH_FILE_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_FLASH_FILE_HOST_H_

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/process.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/host/resource_message_filter.h"

namespace ppapi {
class PepperFilePath;
}

namespace ppapi {
namespace host {
struct ReplyMessageContext;
}
}

namespace content {

class BrowserPpapiHost;

class PepperFlashFileHost : public ppapi::host::ResourceHost {
 public:
  PepperFlashFileHost(BrowserPpapiHost* host,
                      PP_Instance instance,
                      PP_Resource resource);
  virtual ~PepperFlashFileHost();

  static FilePath GetDataDirName(const FilePath& profile_path);

 private:
  DISALLOW_COPY_AND_ASSIGN(PepperFlashFileHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_FLASH_FILE_HOST_H_
