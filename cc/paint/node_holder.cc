// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/node_holder.h"

namespace cc {

NodeHolder::NodeHolder() : tag(EMPTY) {}

NodeHolder::NodeHolder(scoped_refptr<TextHolder> holder)
    : tag(TEXT_HOLDER), text_holder(holder) {}

NodeHolder::NodeHolder(int id) : tag(ID), id(id) {}

NodeHolder::NodeHolder(const NodeHolder& other) {
  text_holder = other.text_holder;
  id = other.id;
  tag = other.tag;
}

NodeHolder::~NodeHolder() = default;

bool operator==(const NodeHolder& l, const NodeHolder& r) {
  if (l.tag != r.tag)
    return false;
  switch (l.tag) {
    case NodeHolder::TEXT_HOLDER:
      return l.text_holder == r.text_holder;
    case NodeHolder::ID:
      return l.id == r.id;
    case NodeHolder::EMPTY:
      return true;
  }
}

bool operator!=(const NodeHolder& l, const NodeHolder& r) {
  return !(l == r);
}

}  // namespace cc
