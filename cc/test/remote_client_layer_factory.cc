// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/remote_client_layer_factory.h"

#include "cc/layers/layer.h"

namespace cc {
namespace {
class ClientLayer : public Layer {
 public:
  explicit ClientLayer(int engine_layer_id) : Layer(engine_layer_id) {}

 private:
  ~ClientLayer() override {}
};
}  // namespace

RemoteClientLayerFactory::RemoteClientLayerFactory() = default;

RemoteClientLayerFactory::~RemoteClientLayerFactory() = default;

scoped_refptr<Layer> RemoteClientLayerFactory::CreateLayer(
    int engine_layer_id) {
  return make_scoped_refptr(new ClientLayer(engine_layer_id));
}

}  // namespace cc
