// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_CRX_FILE_SYSTEM_MESSAGE_FILTER_H_
#define CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_CRX_FILE_SYSTEM_MESSAGE_FILTER_H_

#include <set>
#include <string>

#include "base/files/file_path.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/host/resource_message_filter.h"
#include "url/gurl.h"

class Profile;

namespace content {
class BrowserPpapiHost;
}

namespace ppapi {
namespace host {
struct HostMessageContext;
}  // namespace host
}  // namespace ppapi

namespace chrome {

class PepperCrxFileSystemMessageFilter
    : public ppapi::host::ResourceMessageFilter {
 public:
  static PepperCrxFileSystemMessageFilter* Create(
      PP_Instance instance,
      content::BrowserPpapiHost* host);

  // ppapi::host::ResourceMessageFilter implementation.
  virtual scoped_refptr<base::TaskRunner> OverrideTaskRunnerForMessage(
      const IPC::Message& msg) OVERRIDE;
  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

 private:
  PepperCrxFileSystemMessageFilter(
      int render_process_id,
      const base::FilePath& profile_directory,
      const GURL& document_url);

  virtual ~PepperCrxFileSystemMessageFilter();

  Profile* GetProfile();

  // Returns filesystem id of isolated filesystem if valid, or empty string
  // otherwise.  This must run on the UI thread because ProfileManager only
  // allows access on that thread.
  std::string CreateIsolatedFileSystem(Profile* profile);

  int32_t OnOpenFileSystem(ppapi::host::HostMessageContext* context);

  const int render_process_id_;
  const base::FilePath& profile_directory_;
  const GURL document_url_;

  // Set of origins that can use CrxFs private APIs from NaCl.
  std::set<std::string> allowed_crxfs_origins_;

  DISALLOW_COPY_AND_ASSIGN(PepperCrxFileSystemMessageFilter);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_CRX_FILE_SYSTEM_MESSAGE_FILTER_H_
