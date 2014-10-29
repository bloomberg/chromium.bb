// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/copresence_socket/copresence_socket_resources.h"

#include "base/lazy_instance.h"
#include "components/copresence_sockets/public/copresence_peer.h"
#include "components/copresence_sockets/public/copresence_socket.h"
#include "content/public/browser/browser_context.h"

using copresence_sockets::CopresencePeer;
using copresence_sockets::CopresenceSocket;

namespace extensions {

// CopresencePeerResource.

// static
static base::LazyInstance<
    BrowserContextKeyedAPIFactory<ApiResourceManager<CopresencePeerResource>>>
    g_peer_factory = LAZY_INSTANCE_INITIALIZER;

// static
template <>
BrowserContextKeyedAPIFactory<ApiResourceManager<CopresencePeerResource>>*
ApiResourceManager<CopresencePeerResource>::GetFactoryInstance() {
  return g_peer_factory.Pointer();
}

CopresencePeerResource::CopresencePeerResource(
    const std::string& owner_extension_id,
    scoped_ptr<copresence_sockets::CopresencePeer> peer)
    : ApiResource(owner_extension_id), peer_(peer.Pass()) {
}

CopresencePeerResource::~CopresencePeerResource() {
}

copresence_sockets::CopresencePeer* CopresencePeerResource::peer() {
  return peer_.get();
}

// CopresenceSocketResource.

// static
static base::LazyInstance<
    BrowserContextKeyedAPIFactory<ApiResourceManager<CopresenceSocketResource>>>
    g_socket_factory = LAZY_INSTANCE_INITIALIZER;

// static
template <>
BrowserContextKeyedAPIFactory<ApiResourceManager<CopresenceSocketResource>>*
ApiResourceManager<CopresenceSocketResource>::GetFactoryInstance() {
  return g_socket_factory.Pointer();
}

CopresenceSocketResource::CopresenceSocketResource(
    const std::string& owner_extension_id,
    scoped_ptr<copresence_sockets::CopresenceSocket> socket)
    : ApiResource(owner_extension_id), socket_(socket.Pass()) {
}

CopresenceSocketResource::~CopresenceSocketResource() {
}

}  // namespace extensions
