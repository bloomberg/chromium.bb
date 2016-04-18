// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_LAYER_LIST_ITERATOR_H_
#define CC_LAYERS_LAYER_LIST_ITERATOR_H_

#include <stdlib.h>
#include <vector>

#include "cc/base/cc_export.h"

namespace cc {

class LayerImpl;

// Unlike LayerIterator and friends, these iterators are not intended to permit
// traversing the RSLL. Rather, they visit a collection of LayerImpls in
// stacking order. All recursive walks over the LayerImpl tree should be
// switched to use these classes instead as the concept of a LayerImpl tree is
// deprecated.
class CC_EXPORT LayerListIterator {
 public:
  explicit LayerListIterator(LayerImpl* root_layer);
  LayerListIterator(const LayerListIterator& other);
  virtual ~LayerListIterator();

  bool operator==(const LayerListIterator& other) const {
    return current_layer_ == other.current_layer_;
  }

  bool operator!=(const LayerListIterator& other) const {
    return !(*this == other);
  }

  // We will only support prefix increment.
  virtual LayerListIterator& operator++();
  LayerImpl* operator->() const { return current_layer_; }
  LayerImpl* operator*() const { return current_layer_; }

 protected:
  // The implementation of this iterator is currently tied tightly to the layer
  // tree, but it should be straightforward to reimplement in terms of a list
  // when it's ready.
  LayerImpl* current_layer_;
  std::vector<size_t> list_indices_;
};

class CC_EXPORT LayerListReverseIterator : public LayerListIterator {
 public:
  explicit LayerListReverseIterator(LayerImpl* root_layer);
  ~LayerListReverseIterator() override;

  // We will only support prefix increment.
  LayerListIterator& operator++() override;

 private:
  void DescendToRightmostInSubtree();
};

}  // namespace cc

#endif  // CC_LAYERS_LAYER_LIST_ITERATOR_H_
