// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_RENDERER_CONNECTION_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_RENDERER_CONNECTION_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/public/browser/browser_message_filter.h"
#include "ppapi/c/pp_instance.h"

namespace ppapi {
namespace proxy {
class ResourceMessageCallParams;
}
}

namespace content {

// This class represents a connection from the browser to the renderer for
// sending/receiving pepper ResourceHost related messages. When the browser
// and renderer communicate about ResourceHosts, they should pass the plugin
// process ID to identify which plugin they are talking about.
class PepperRendererConnection : public BrowserMessageFilter {
 public:
  PepperRendererConnection();

  // BrowserMessageFilter overrides.
  virtual bool OnMessageReceived(const IPC::Message& msg,
                                 bool* message_was_ok) OVERRIDE;

 private:
  virtual ~PepperRendererConnection();

  void OnMsgCreateResourceHostFromHost(
      int child_process_id,
      const ppapi::proxy::ResourceMessageCallParams& params,
      PP_Instance instance,
      const IPC::Message& nested_msg);

  DISALLOW_COPY_AND_ASSIGN(PepperRendererConnection);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_RENDERER_CONNECTION_H_
