// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREE_SYNCHRONIZER_H_
#define CC_TREE_SYNCHRONIZER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "cc/cc_export.h"
#include "cc/scoped_ptr_hash_map.h"

namespace cc {

class LayerImpl;
class LayerTreeImpl;
class Layer;

class CC_EXPORT TreeSynchronizer {
public:
    // Accepts a Layer tree and returns a reference to a LayerImpl tree that duplicates the structure
    // of the Layer tree, reusing the LayerImpls in the tree provided by oldLayerImplRoot if possible.
    static scoped_ptr<LayerImpl> synchronizeTrees(Layer* layerRoot, scoped_ptr<LayerImpl> oldLayerImplRoot, LayerTreeImpl*);
    static scoped_ptr<LayerImpl> synchronizeTrees(LayerImpl* layerRoot, scoped_ptr<LayerImpl> oldLayerImplRoot, LayerTreeImpl*);

    // Pushes properties from a Layer or LayerImpl tree to a structurally equivalent
    // LayerImpl tree.
    static void pushProperties(Layer* layerRoot, LayerImpl* layerImplRoot);
    static void pushProperties(LayerImpl* layerRoot, LayerImpl* layerImplRoot);

private:
    TreeSynchronizer(); // Not instantiable.

    DISALLOW_COPY_AND_ASSIGN(TreeSynchronizer);
};

} // namespace cc

#endif  // CC_TREE_SYNCHRONIZER_H_
