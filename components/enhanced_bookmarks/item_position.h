// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENHANCED_BOOKMARKS_ITEM_POSITION_H_
#define COMPONENTS_ENHANCED_BOOKMARKS_ITEM_POSITION_H_

#include <string>
#include <vector>

namespace enhanced_bookmarks {

// Convenience class for generating string based relative ordering between
// bookmark nodes.
class ItemPosition {
 public:
  ~ItemPosition();

  // Creates a position suitable to use as a starting point.
  static ItemPosition CreateInitialPosition();

  // Creates positions relative to other positions.
  static ItemPosition CreateBefore(const ItemPosition& other);
  static ItemPosition CreateBetween(const ItemPosition& before,
                                    const ItemPosition& after);
  static ItemPosition CreateAfter(const ItemPosition& other);

  bool IsValid() const;

  // Returns a string representation of the position. The string representations
  // of two position have the same ordering as the positions themselves when
  // compared using ASCII order.
  std::string ToString() const;

  // Comparison functions.
  bool Equals(const ItemPosition& other) const;
  bool LessThan(const ItemPosition& other) const;

 private:
  typedef std::vector<unsigned char> PositionVector;

  ItemPosition();
  explicit ItemPosition(const PositionVector& position);

  static PositionVector CreateBeforeImpl(const PositionVector& before,
                                         size_t from_index);
  static PositionVector CreateBetweenImpl(const PositionVector& before,
                                          const PositionVector& after);
  static PositionVector CreateAfterImpl(const PositionVector& after,
                                        size_t from_index);

  PositionVector position_;
};

}  // namespace enhanced_bookmarks

#endif  // COMPONENTS_ENHANCED_BOOKMARKS_ITEM_POSITION_H_
