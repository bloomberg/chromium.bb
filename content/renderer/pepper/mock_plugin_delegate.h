// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_MOCK_PLUGIN_DELEGATE_H_
#define CONTENT_RENDERER_PEPPER_MOCK_PLUGIN_DELEGATE_H_

#include "content/renderer/pepper/plugin_delegate.h"

struct PP_NetAddress_Private;
namespace ppapi { class PPB_X509Certificate_Fields; }

namespace content {

class MockPluginDelegate : public PluginDelegate {
 public:
  MockPluginDelegate();
  virtual ~MockPluginDelegate();

  virtual void PluginFocusChanged(PepperPluginInstanceImpl* instance,
                                  bool focused) OVERRIDE;
  virtual void PluginTextInputTypeChanged(
      PepperPluginInstanceImpl* instance) OVERRIDE;
  virtual void PluginCaretPositionChanged(
      PepperPluginInstanceImpl* instance) OVERRIDE;
  virtual void PluginRequestedCancelComposition(
      PepperPluginInstanceImpl* instance) OVERRIDE;
  virtual void PluginSelectionChanged(
      PepperPluginInstanceImpl* instance) OVERRIDE;
  virtual void InstanceCreated(PepperPluginInstanceImpl* instance) OVERRIDE;
  virtual void InstanceDeleted(PepperPluginInstanceImpl* instance) OVERRIDE;
  virtual bool AsyncOpenFile(const base::FilePath& path,
                             int flags,
                             const AsyncOpenFileCallback& callback) OVERRIDE;
  virtual scoped_refptr<base::MessageLoopProxy>
      GetFileThreadMessageLoopProxy() OVERRIDE;

  // Add/remove a network list observer.
  virtual void SampleGamepads(WebKit::WebGamepads* data) OVERRIDE;
  virtual void HandleDocumentLoad(
      PepperPluginInstanceImpl* instance,
      const WebKit::WebURLResponse& response) OVERRIDE;
  virtual RendererPpapiHost* CreateExternalPluginModule(
      scoped_refptr<PluginModule> module,
      const base::FilePath& path,
      ::ppapi::PpapiPermissions permissions,
      const IPC::ChannelHandle& channel_handle,
      base::ProcessId plugin_pid,
      int plugin_child_id) OVERRIDE;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_MOCK_PLUGIN_DELEGATE_H_
