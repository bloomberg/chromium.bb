// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/resource_converter.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/pepper/pepper_file_system_host.h"
#include "ipc/ipc_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/resource_var.h"
#include "ppapi/shared_impl/scoped_pp_var.h"
#include "third_party/WebKit/public/platform/WebFileSystem.h"
#include "third_party/WebKit/public/web/WebDOMFileSystem.h"

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

// Converts a blink::WebFileSystem::Type to a PP_FileSystemType.
PP_FileSystemType WebFileSystemTypeToPPAPI(blink::WebFileSystem::Type type) {
  switch (type) {
    case blink::WebFileSystem::TypeTemporary:
      return PP_FILESYSTEMTYPE_LOCALTEMPORARY;
    case blink::WebFileSystem::TypePersistent:
      return PP_FILESYSTEMTYPE_LOCALPERSISTENT;
    case blink::WebFileSystem::TypeIsolated:
      return PP_FILESYSTEMTYPE_ISOLATED;
    case blink::WebFileSystem::TypeExternal:
      return PP_FILESYSTEMTYPE_EXTERNAL;
    default:
      NOTREACHED();
      return PP_FILESYSTEMTYPE_LOCALTEMPORARY;
  }
}

// Given a V8 value containing a DOMFileSystem, creates a resource host and
// returns the resource information for serialization.
// On error, false.
bool DOMFileSystemToResource(
    PP_Instance instance,
    content::RendererPpapiHost* host,
    const blink::WebDOMFileSystem& dom_file_system,
    int* pending_renderer_id,
    scoped_ptr<IPC::Message>* create_message,
    scoped_ptr<IPC::Message>* browser_host_create_message) {
  DCHECK(!dom_file_system.isNull());

  PP_FileSystemType file_system_type =
      WebFileSystemTypeToPPAPI(dom_file_system.type());
  GURL root_url = dom_file_system.rootURL();

  // External file systems are not currently supported. (Without this check,
  // there would be a CHECK-fail in FileRefResource.)
  // TODO(mgiuca): Support external file systems.
  if (file_system_type == PP_FILESYSTEMTYPE_EXTERNAL)
    return false;

  *pending_renderer_id = host->GetPpapiHost()->AddPendingResourceHost(
      scoped_ptr<ppapi::host::ResourceHost>(
          new content::PepperFileSystemHost(host, instance, 0, root_url,
                                            file_system_type)));
  if (*pending_renderer_id == 0)
    return false;

  create_message->reset(
      new PpapiPluginMsg_FileSystem_CreateFromPendingHost(file_system_type));

  browser_host_create_message->reset(
      new PpapiHostMsg_FileSystem_CreateFromRenderer(root_url.spec(),
                                                     file_system_type));
  return true;
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

  blink::WebDOMFileSystem dom_file_system =
      blink::WebDOMFileSystem::fromV8Value(val);
  if (!dom_file_system.isNull()) {
    int pending_renderer_id;
    scoped_ptr<IPC::Message> create_message;
    scoped_ptr<IPC::Message> browser_host_create_message;
    if (!DOMFileSystemToResource(instance_, host_, dom_file_system,
                                 &pending_renderer_id, &create_message,
                                 &browser_host_create_message)) {
      return false;
    }
    DCHECK(create_message);
    DCHECK(browser_host_create_message);
    scoped_refptr<HostResourceVar> result_var =
        CreateResourceVarWithBrowserHost(
            pending_renderer_id, *create_message, *browser_host_create_message);
    *result = result_var->GetPPVar();
    *was_resource = true;
    return true;
  }

  // The value was not convertible to a resource. Return true with
  // |was_resource| set to false. As per the interface of FromV8Value, |result|
  // may be left unmodified in this case.
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
