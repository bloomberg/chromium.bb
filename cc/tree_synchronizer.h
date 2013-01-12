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

    // Pushes properties from a Layer tree to a structurally equivalent
    // LayerImpl tree.
    static void pushProperties(Layer* layerRoot, LayerImpl* layerImplRoot);

private:
    TreeSynchronizer(); // Not instantiable.

    typedef ScopedPtrHashMap<int, LayerImpl> ScopedPtrLayerImplMap;
    typedef base::hash_map<int, LayerImpl*> RawPtrLayerImplMap;

    // Declared as static member functions so they can access functions on Layer as a friend class.
    static scoped_ptr<LayerImpl> reuseOrCreateLayerImpl(RawPtrLayerImplMap& newLayers, ScopedPtrLayerImplMap& oldLayers, Layer*, LayerTreeImpl*);
    static void collectExistingLayerImplRecursive(ScopedPtrLayerImplMap& oldLayers, scoped_ptr<LayerImpl>);
    static scoped_ptr<LayerImpl> synchronizeTreeRecursive(RawPtrLayerImplMap& newLayers, ScopedPtrLayerImplMap& oldLayers, Layer*, LayerTreeImpl*);
    static void updateScrollbarLayerPointersRecursive(const RawPtrLayerImplMap& newLayers, Layer*);

    DISALLOW_COPY_AND_ASSIGN(TreeSynchronizer);
};

} // namespace cc

#endif  // CC_TREE_SYNCHRONIZER_H_
