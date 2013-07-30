// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/mock_plugin_delegate.h"

#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/plugin_delegate.h"
#include "content/renderer/pepper/plugin_module.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#include "ppapi/shared_impl/ppapi_preferences.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"

namespace content {

MockPluginDelegate::MockPluginDelegate() {
}

MockPluginDelegate::~MockPluginDelegate() {
}

void MockPluginDelegate::PluginFocusChanged(PepperPluginInstanceImpl* instance,
                                            bool focused) {
}

void MockPluginDelegate::PluginTextInputTypeChanged(
    PepperPluginInstanceImpl* instance) {
}

void MockPluginDelegate::PluginCaretPositionChanged(
    PepperPluginInstanceImpl* instance) {
}

void MockPluginDelegate::PluginRequestedCancelComposition(
    PepperPluginInstanceImpl* instance) {
}

void MockPluginDelegate::PluginSelectionChanged(
    PepperPluginInstanceImpl* instance) {
}

void MockPluginDelegate::InstanceCreated(PepperPluginInstanceImpl* instance) {
}

void MockPluginDelegate::InstanceDeleted(PepperPluginInstanceImpl* instance) {
}

bool MockPluginDelegate::AsyncOpenFile(const base::FilePath& path,
                                       int flags,
                                       const AsyncOpenFileCallback& callback) {
  return false;
}

scoped_refptr<base::MessageLoopProxy>
MockPluginDelegate::GetFileThreadMessageLoopProxy() {
  return scoped_refptr<base::MessageLoopProxy>();
}

void MockPluginDelegate::SampleGamepads(WebKit::WebGamepads* data) {
  data->length = 0;
}

void MockPluginDelegate::HandleDocumentLoad(
    PepperPluginInstanceImpl* instance,
    const WebKit::WebURLResponse& response) {
}

RendererPpapiHost* MockPluginDelegate::CreateExternalPluginModule(
    scoped_refptr<PluginModule> module,
    const base::FilePath& path,
    ::ppapi::PpapiPermissions permissions,
    const IPC::ChannelHandle& channel_handle,
    base::ProcessId plugin_pid,
    int plugin_child_id) {
  return NULL;
}

}  // namespace content
