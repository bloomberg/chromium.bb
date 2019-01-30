// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_NODE_HOLDER_H_
#define CC_PAINT_NODE_HOLDER_H_

#include "base/memory/scoped_refptr.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/text_holder.h"

namespace cc {

// This struct is used to hold the information of node that PaintOp associates
// with, either the TextHolder or the Id could be set, but only one will finally
// be supported base on the performance impact.
struct CC_PAINT_EXPORT NodeHolder {
  NodeHolder();
  explicit NodeHolder(scoped_refptr<TextHolder> text_holder);
  explicit NodeHolder(int id);
  NodeHolder(const NodeHolder& node_holder);
  virtual ~NodeHolder();

  bool IsEmpty() const { return tag == EMPTY; }

  enum { EMPTY, TEXT_HOLDER, ID } tag;

  scoped_refptr<TextHolder> text_holder;
  int id;
};

bool operator==(const NodeHolder& l, const NodeHolder& r);

bool operator!=(const NodeHolder& l, const NodeHolder& r);

}  // namespace cc

#endif  // CC_PAINT_NODE_HOLDER_H_
