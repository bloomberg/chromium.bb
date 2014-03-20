// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_UI_RESOURCE_LAYER_TREE_HOST_IMPL_H_
#define CC_TEST_FAKE_UI_RESOURCE_LAYER_TREE_HOST_IMPL_H_

#include "base/containers/hash_tables.h"
#include "cc/test/fake_layer_tree_host_impl.h"

namespace cc {

class FakeUIResourceLayerTreeHostImpl : public FakeLayerTreeHostImpl {
 public:
  explicit FakeUIResourceLayerTreeHostImpl(Proxy* proxy,
                                           SharedBitmapManager* manager);
  virtual ~FakeUIResourceLayerTreeHostImpl();

  virtual void CreateUIResource(UIResourceId uid,
                                const UIResourceBitmap& bitmap) OVERRIDE;

  virtual void DeleteUIResource(UIResourceId uid) OVERRIDE;

  virtual ResourceProvider::ResourceId ResourceIdForUIResource(
      UIResourceId uid) const OVERRIDE;

  virtual bool IsUIResourceOpaque(UIResourceId uid) const OVERRIDE;

 private:
  ResourceProvider::ResourceId fake_next_resource_id_;
  typedef base::hash_map<UIResourceId, LayerTreeHostImpl::UIResourceData>
      UIResourceMap;
  UIResourceMap fake_ui_resource_map_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_UI_RESOURCE_LAYER_TREE_HOST_IMPL_H_
