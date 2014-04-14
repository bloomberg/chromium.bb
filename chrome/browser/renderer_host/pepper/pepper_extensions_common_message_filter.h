// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_EXTENSIONS_COMMON_MESSAGE_FILTER_H_
#define CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_EXTENSIONS_COMMON_MESSAGE_FILTER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "extensions/browser/extension_function.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_message_filter.h"
#include "url/gurl.h"

struct ExtensionHostMsg_Request_Params;

namespace base {
class ListValue;
}

namespace content {
class BrowserPpapiHost;
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
                                      int render_frame_id,
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
  // DispatcherOwner holds an ExtensionFunctionDispatcher instance and acts as
  // its delegate. It is designed to meet the lifespan requirements of
  // ExtensionFunctionDispatcher and its delegate. (Please see the comment of
  // ExtensionFunctionDispatcher constructor in
  // extension_function_dispatcher.h.)
  class DispatcherOwner;

  int32_t OnPost(ppapi::host::HostMessageContext* context,
                 const std::string& request_name,
                 base::ListValue& args);

  int32_t OnCall(ppapi::host::HostMessageContext* context,
                 const std::string& request_name,
                 base::ListValue& args);

  // Returns true if |dispatcher_owner_| is non-null.
  bool EnsureDispatcherOwnerInitialized();
  // Resets |dispatcher_owner_| to NULL.
  void DetachDispatcherOwner();

  void PopulateParams(const std::string& request_name,
                      base::ListValue* args,
                      bool has_callback,
                      ExtensionHostMsg_Request_Params* params);

  void OnCallCompleted(ppapi::host::ReplyMessageContext reply_context,
                       ExtensionFunction::ResponseType type,
                       const base::ListValue& results,
                       const std::string& error);

  // All the members are initialized on the IO thread when the object is
  // constructed, and accessed only on the UI thread afterwards.
  int render_process_id_;
  int render_frame_id_;
  base::FilePath profile_directory_;
  GURL document_url_;

  // Not-owning pointer. It will be set to NULL when it goes away.
  DispatcherOwner* dispatcher_owner_;
  bool dispatcher_owner_initialized_;

  DISALLOW_COPY_AND_ASSIGN(PepperExtensionsCommonMessageFilter);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_EXTENSIONS_COMMON_MESSAGE_FILTER_H_
