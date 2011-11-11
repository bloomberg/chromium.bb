// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/java/java_bridge_dispatcher_host.h"

#include "content/browser/renderer_host/browser_render_process_host.h"
#include "content/browser/renderer_host/java/java_bridge_channel_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/common/child_process.h"
#include "content/common/java_bridge_messages.h"
#include "content/common/npobject_stub.h"
#include "content/common/npobject_util.h"  // For CreateNPVariantParam()
#include "content/public/browser/browser_thread.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"

using content::BrowserThread;

JavaBridgeDispatcherHost::JavaBridgeDispatcherHost(
    RenderViewHost* render_view_host)
    : render_view_host_(render_view_host) {
  DCHECK(render_view_host_);
}

JavaBridgeDispatcherHost::~JavaBridgeDispatcherHost() {
}

void JavaBridgeDispatcherHost::AddNamedObject(const string16& name,
                                              NPObject* object) {
  // Post to the WEBKIT thread, as that's where we want injected objects to
  // live.
  WebKit::WebBindings::retainObject(object);
  BrowserThread::PostTask(
      BrowserThread::WEBKIT,
      FROM_HERE,
      NewRunnableMethod(this,
                        &JavaBridgeDispatcherHost::DoAddNamedObject,
                        name,
                        object));
}

void JavaBridgeDispatcherHost::RemoveNamedObject(const string16& name) {
  // On receipt of this message, the JavaBridgeDispatcher will drop its
  // reference to the corresponding proxy object. When the last reference is
  // removed, the proxy object will delete its NPObjectProxy, which will cause
  // the NPObjectStub to be deleted, which will drop its reference to the
  // original NPObject.
  render_view_host_->Send(new JavaBridgeMsg_RemoveNamedObject(
      render_view_host_->routing_id(), name));
}

void JavaBridgeDispatcherHost::DoAddNamedObject(const string16& name,
                                                NPObject* object) {
  EnsureChannelIsSetUp();

  NPVariant variant;
  OBJECT_TO_NPVARIANT(object, variant);
  DCHECK_EQ(variant.type, NPVariantType_Object);

  // Create an NPVariantParam suitable for serialization over IPC from our
  // NPVariant. Internally, this will create an NPObjectStub, which takes a
  // reference to the underlying NPObject. We pass release = true to release
  // the ref added in AddNamedObject(). We don't need the containing window or
  // the page URL, as we don't do re-entrant sync IPC.
  NPVariant_Param variant_param;
  CreateNPVariantParam(variant, channel_, &variant_param, true, 0, GURL());
  DCHECK_EQ(variant_param.type, NPVARIANT_PARAM_SENDER_OBJECT_ROUTING_ID);

  render_view_host_->Send(new JavaBridgeMsg_AddNamedObject(
      render_view_host_->routing_id(), name, variant_param));
}

void JavaBridgeDispatcherHost::EnsureChannelIsSetUp() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  if (channel_) {
    return;
  }

  channel_ = JavaBridgeChannelHost::GetJavaBridgeChannelHost(
      render_view_host_->process()->id(),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));

  // The channel creates the channel handle based on the renderer ID we passed
  // to GetJavaBridgeChannelHost() and, on POSIX, the file descriptor used by
  // the underlying channel.
  IPC::ChannelHandle channel_handle = channel_->channel_handle();

  bool sent = render_view_host_->Send(new JavaBridgeMsg_Init(
      render_view_host_->routing_id(), channel_handle));
  DCHECK(sent);
}
