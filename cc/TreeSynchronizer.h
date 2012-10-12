// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TreeSynchronizer_h
#define TreeSynchronizer_h

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "cc/scoped_ptr_hash_map.h"

namespace cc {

class CCLayerImpl;
class CCLayerTreeHostImpl;
class LayerChromium;

class TreeSynchronizer {
public:
    // Accepts a LayerChromium tree and returns a reference to a CCLayerImpl tree that duplicates the structure
    // of the LayerChromium tree, reusing the CCLayerImpls in the tree provided by oldCCLayerImplRoot if possible.
    static scoped_ptr<CCLayerImpl> synchronizeTrees(LayerChromium* layerRoot, scoped_ptr<CCLayerImpl> oldCCLayerImplRoot, CCLayerTreeHostImpl*);

private:
    TreeSynchronizer(); // Not instantiable.

    typedef ScopedPtrHashMap<int, CCLayerImpl> ScopedPtrCCLayerImplMap;
    typedef base::hash_map<int, CCLayerImpl*> RawPtrCCLayerImplMap;

    // Declared as static member functions so they can access functions on LayerChromium as a friend class.
    static scoped_ptr<CCLayerImpl> reuseOrCreateCCLayerImpl(RawPtrCCLayerImplMap& newLayers, ScopedPtrCCLayerImplMap& oldLayers, LayerChromium*);
    static void collectExistingCCLayerImplRecursive(ScopedPtrCCLayerImplMap& oldLayers, scoped_ptr<CCLayerImpl>);
    static scoped_ptr<CCLayerImpl> synchronizeTreeRecursive(RawPtrCCLayerImplMap& newLayers, ScopedPtrCCLayerImplMap& oldLayers, LayerChromium*, CCLayerTreeHostImpl*);
    static void updateScrollbarLayerPointersRecursive(const RawPtrCCLayerImplMap& newLayers, LayerChromium*);

    DISALLOW_COPY_AND_ASSIGN(TreeSynchronizer);
};

} // namespace cc

#endif // TreeSynchronizer_h
