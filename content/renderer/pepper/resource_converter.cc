// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/resource_converter.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "ipc/ipc_message.h"
#include "ppapi/shared_impl/scoped_pp_var.h"

namespace {

void FlushComplete(
    const base::Callback<void(bool)>& callback,
    const std::vector<PP_Var>& browser_host_resource_vars,
    const std::vector<int>& pending_host_ids) {
  CHECK(browser_host_resource_vars.size() == pending_host_ids.size());
  for (size_t i = 0; i < browser_host_resource_vars.size(); ++i) {
    // TODO(raymes): Set the pending browser host ID in the resource var.
  }
  callback.Run(true);
}

}  // namespace

namespace content {

ResourceConverter::~ResourceConverter() {}

ResourceConverterImpl::ResourceConverterImpl(PP_Instance instance,
                                             RendererPpapiHost* host)
    : instance_(instance),
      host_(host) {
}

ResourceConverterImpl::~ResourceConverterImpl() {
  // Verify Flush() was called.
  DCHECK(browser_host_create_messages_.empty());
  DCHECK(browser_host_resource_vars.empty());
}

bool ResourceConverterImpl::FromV8Value(v8::Handle<v8::Value> val,
                                        v8::Handle<v8::Context> context,
                                        PP_Var* result) {

  return false;
}

void ResourceConverterImpl::Flush(const base::Callback<void(bool)>& callback) {
  host_->CreateBrowserResourceHosts(
      instance_,
      browser_host_create_messages_,
      base::Bind(&FlushComplete, callback, browser_host_resource_vars));
  browser_host_create_messages_.clear();
  browser_host_resource_vars.clear();
}

PP_Var ResourceConverterImpl::CreateResourceVar(
    const IPC::Message& create_message) {
  // TODO(raymes): Create a ResourceVar here.
  return PP_MakeUndefined();
}

PP_Var ResourceConverterImpl::CreateResourceVarWithBrowserHost(
    const IPC::Message& create_message,
    const IPC::Message& browser_host_create_message) {
  PP_Var result = CreateResourceVar(create_message);
  browser_host_create_messages_.push_back(browser_host_create_message);
  browser_host_resource_vars.push_back(result);
  return result;
}

}  // namespace content
