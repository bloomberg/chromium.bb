// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_EXTENSIONS_COMMON_MESSAGE_FILTER_H_
#define CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_EXTENSIONS_COMMON_MESSAGE_FILTER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "googleurl/src/gurl.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/host/resource_message_filter.h"

namespace base {
class ListValue;
}

namespace content {
class BrowserPpapiHost;
}

namespace ppapi {
namespace host {
struct HostMessageContext;
}
}

namespace chrome {

class PepperExtensionsCommonMessageFilter
    : public ppapi::host::ResourceMessageFilter {
 public:
  static PepperExtensionsCommonMessageFilter* Create(
      content::BrowserPpapiHost* host,
      PP_Instance instance);

 protected:
  PepperExtensionsCommonMessageFilter(int render_process_id,
                                      int render_view_id,
                                      const base::FilePath& profile_directory,
                                      const GURL& document_url);
  virtual ~PepperExtensionsCommonMessageFilter();

  // ppapi::host::ResourceMessageFilter overrides.
  virtual scoped_refptr<base::TaskRunner> OverrideTaskRunnerForMessage(
      const IPC::Message& msg) OVERRIDE;
  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

 private:
  int32_t OnPost(ppapi::host::HostMessageContext* context,
                 const std::string& request_name,
                 base::ListValue& args);

  int32_t OnCall(ppapi::host::HostMessageContext* context,
                 const std::string& request_name,
                 base::ListValue& args);

  // All the members are initialized on the IO thread when the object is
  // constructed, and accessed only on the UI thread afterwards.
  int render_process_id_;
  int render_view_id_;
  base::FilePath profile_directory_;
  GURL document_url_;

  DISALLOW_COPY_AND_ASSIGN(PepperExtensionsCommonMessageFilter);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_EXTENSIONS_COMMON_MESSAGE_FILTER_H_
