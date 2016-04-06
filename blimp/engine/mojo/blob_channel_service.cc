// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/mojo/blob_channel_service.h"

namespace blimp {
namespace engine {

BlobChannelService::BlobChannelService(mojom::BlobChannelRequest request)
    : binding_(this, std::move(request)) {}

BlobChannelService::~BlobChannelService() {}

void BlobChannelService::Put(const mojo::String& id,
                             mojo::Array<uint8_t> data) {
  NOTIMPLEMENTED();
}

void BlobChannelService::Push(const mojo::String& id) {
  NOTIMPLEMENTED();
}

// static
void BlobChannelService::Create(
    mojo::InterfaceRequest<mojom::BlobChannel> request) {
  // Object lifetime is managed by BlobChannelService's StrongBinding
  // |binding_|.
  new BlobChannelService(std::move(request));
}

}  // namespace engine
}  // namespace blimp
