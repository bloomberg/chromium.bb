// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_list_iterator.h"

#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"

namespace cc {

template <typename LayerType>
LayerListIterator<LayerType>::LayerListIterator(LayerType* root_layer)
    : current_layer_(root_layer) {
  DCHECK(!root_layer || !root_layer->parent());
  list_indices_.push_back(0);
}

template <typename LayerType>
LayerListIterator<LayerType>::LayerListIterator(
    const LayerListIterator<LayerType>& other) = default;

template <typename LayerType>
LayerListIterator<LayerType>::~LayerListIterator() {}

template <typename LayerType>
LayerListIterator<LayerType>& LayerListIterator<LayerType>::operator++() {
  // case 0: done
  if (!current_layer_)
    return *this;

  // case 1: descend.
  if (!current_layer_->children().empty()) {
    current_layer_ = current_layer_->child_at(0);
    list_indices_.push_back(0);
    return *this;
  }

  for (LayerType* parent = current_layer_->parent(); parent;
       parent = parent->parent()) {
    // We now try and advance in some list of siblings.
    // case 2: Advance to a sibling.
    if (list_indices_.back() + 1 < parent->children().size()) {
      ++list_indices_.back();
      current_layer_ = parent->child_at(list_indices_.back());
      return *this;
    }

    // We need to ascend. We will pop an index off the stack.
    list_indices_.pop_back();
  }

  current_layer_ = nullptr;
  return *this;
}

template <typename LayerType>
LayerListReverseIterator<LayerType>::LayerListReverseIterator(
    LayerType* root_layer)
    : LayerListIterator<LayerType>(root_layer) {
  DescendToRightmostInSubtree();
}

template <typename LayerType>
LayerListReverseIterator<LayerType>::~LayerListReverseIterator() {}

// We will only support prefix increment.
template <typename LayerType>
LayerListIterator<LayerType>& LayerListReverseIterator<LayerType>::
operator++() {
  // case 0: done
  if (!current_layer())
    return *this;

  // case 1: we're the leftmost sibling.
  if (!list_indices().back()) {
    list_indices().pop_back();
    this->current_layer_ = current_layer()->parent();
    return *this;
  }

  // case 2: we're not the leftmost sibling. In this case, we want to move one
  // sibling over, and then descend to the rightmost descendant in that subtree.
  CHECK(current_layer()->parent());
  --list_indices().back();
  this->current_layer_ =
      current_layer()->parent()->child_at(list_indices().back());
  DescendToRightmostInSubtree();
  return *this;
}

template <typename LayerType>
void LayerListReverseIterator<LayerType>::DescendToRightmostInSubtree() {
  if (!current_layer())
    return;

  if (current_layer()->children().empty())
    return;

  size_t last_index = current_layer()->children().size() - 1;
  this->current_layer_ = current_layer()->child_at(last_index);
  list_indices().push_back(last_index);
  DescendToRightmostInSubtree();
}

template class LayerListIterator<Layer>;
template class LayerListIterator<LayerImpl>;
template class LayerListReverseIterator<Layer>;
template class LayerListReverseIterator<LayerImpl>;

}  // namespace cc
