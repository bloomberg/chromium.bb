// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TreeSynchronizer_h
#define TreeSynchronizer_h

#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace cc {

class CCLayerImpl;
class CCLayerTreeHostImpl;
class LayerChromium;

class TreeSynchronizer {
WTF_MAKE_NONCOPYABLE(TreeSynchronizer);
public:
    // Accepts a LayerChromium tree and returns a reference to a CCLayerImpl tree that duplicates the structure
    // of the LayerChromium tree, reusing the CCLayerImpls in the tree provided by oldCCLayerImplRoot if possible.
    static PassOwnPtr<CCLayerImpl> synchronizeTrees(LayerChromium* layerRoot, PassOwnPtr<CCLayerImpl> oldCCLayerImplRoot, CCLayerTreeHostImpl*);

private:
    TreeSynchronizer(); // Not instantiable.

    typedef HashMap<int, OwnPtr<CCLayerImpl> > OwnPtrCCLayerImplMap;
    typedef HashMap<int, CCLayerImpl*> RawPtrCCLayerImplMap;

    // Declared as static member functions so they can access functions on LayerChromium as a friend class.
    static PassOwnPtr<CCLayerImpl> reuseOrCreateCCLayerImpl(RawPtrCCLayerImplMap& newLayers, OwnPtrCCLayerImplMap& oldLayers, LayerChromium*);
    static void collectExistingCCLayerImplRecursive(OwnPtrCCLayerImplMap& oldLayers, PassOwnPtr<CCLayerImpl>);
    static PassOwnPtr<CCLayerImpl> synchronizeTreeRecursive(RawPtrCCLayerImplMap& newLayers, OwnPtrCCLayerImplMap& oldLayers, LayerChromium*, CCLayerTreeHostImpl*);
    static void updateScrollbarLayerPointersRecursive(const RawPtrCCLayerImplMap& newLayers, LayerChromium*);
};

} // namespace cc

#endif // TreeSynchronizer_h
