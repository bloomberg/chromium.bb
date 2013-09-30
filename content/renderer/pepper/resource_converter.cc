// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/resource_converter.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "ipc/ipc_message.h"
#include "ppapi/shared_impl/resource_var.h"
#include "ppapi/shared_impl/scoped_pp_var.h"

namespace {

void FlushComplete(
    const base::Callback<void(bool)>& callback,
    const std::vector<scoped_refptr<content::HostResourceVar> >& browser_vars,
    const std::vector<int>& pending_host_ids) {
  CHECK(browser_vars.size() == pending_host_ids.size());
  for (size_t i = 0; i < browser_vars.size(); ++i) {
    browser_vars[i]->set_pending_browser_host_id(pending_host_ids[i]);
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
  DCHECK(browser_vars.empty());
}

bool ResourceConverterImpl::FromV8Value(v8::Handle<v8::Object> val,
                                        v8::Handle<v8::Context> context,
                                        PP_Var* result,
                                        bool* was_resource) {
  v8::Context::Scope context_scope(context);
  v8::HandleScope handle_scope(context->GetIsolate());

  *was_resource = false;
  // TODO(mgiuca): There are currently no values which can be converted to
  // resources.

  return true;
}

void ResourceConverterImpl::Flush(const base::Callback<void(bool)>& callback) {
  host_->CreateBrowserResourceHosts(
      instance_,
      browser_host_create_messages_,
      base::Bind(&FlushComplete, callback, browser_vars));
  browser_host_create_messages_.clear();
  browser_vars.clear();
}

scoped_refptr<HostResourceVar> ResourceConverterImpl::CreateResourceVar(
    int pending_renderer_id,
    const IPC::Message& create_message) {
  return new HostResourceVar(pending_renderer_id, create_message);
}

scoped_refptr<HostResourceVar>
ResourceConverterImpl::CreateResourceVarWithBrowserHost(
    int pending_renderer_id,
    const IPC::Message& create_message,
    const IPC::Message& browser_host_create_message) {
  scoped_refptr<HostResourceVar> result =
      CreateResourceVar(pending_renderer_id, create_message);
  browser_host_create_messages_.push_back(browser_host_create_message);
  browser_vars.push_back(result);
  return result;
}

}  // namespace content
