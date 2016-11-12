// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_SERIALIZATION_TEST_UTILS_H_
#define CC_TEST_SERIALIZATION_TEST_UTILS_H_

#include "base/macros.h"

namespace cc {
class CompositorStateDeserializer;
class FakePictureLayer;
class Layer;
class LayerTree;
class SolidColorScrollbarLayer;

void VerifySerializedTreesAreIdentical(
    LayerTree* engine_layer_tree,
    LayerTree* client_layer_tree,
    CompositorStateDeserializer* compositor_state_deserializer);

void VerifySerializedLayersAreIdentical(
    Layer* engine_layer,
    CompositorStateDeserializer* compositor_state_deserializer);

void VerifySerializedScrollbarLayersAreIdentical(
    SolidColorScrollbarLayer* engine_layer,
    CompositorStateDeserializer* compositor_state_deserializer);

void VerifySerializedPictureLayersAreIdentical(
    FakePictureLayer* engine_layer,
    CompositorStateDeserializer* compositor_state_deserializer);

}  // namespace cc

#endif  // CC_TEST_SERIALIZATION_TEST_UTILS_H_
