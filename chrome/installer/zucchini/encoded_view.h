// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_ZUCCHINI_ENCODED_VIEW_H_
#define CHROME_INSTALLER_ZUCCHINI_ENCODED_VIEW_H_

#include <stddef.h>
#include <stdint.h>

#include <iterator>

#include "chrome/installer/zucchini/image_index.h"
#include "chrome/installer/zucchini/image_utils.h"

namespace zucchini {

constexpr size_t kReferencePaddingProjection = 256;
constexpr size_t kBaseReferenceProjection = 257;

// A Range (providing a begin and end iterator) that adapts ImageIndex to make
// image data appear as an Encoded Image, that is encoded data under a higher
// level of abstraction than raw bytes. In particular:
// - First byte of each reference becomes a projection of its type and Label.
// - Subsequent bytes of each reference becomes |kReferencePaddingProjection|.
// - Non-reference raw bytes remain as raw bytes.
class EncodedView {
 public:
  // RandomAccessIterator whose values are the results of Projection().
  class Iterator {
   public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = size_t;
    using difference_type = ptrdiff_t;
    using reference = size_t;
    using pointer = size_t*;

    Iterator(const EncodedView& encoded_view, difference_type pos)
        : encoded_view_(encoded_view), pos_(pos) {}

    value_type operator*() const {
      return encoded_view_.Projection(static_cast<offset_t>(pos_));
    }

    value_type operator[](difference_type n) const {
      return encoded_view_.Projection(static_cast<offset_t>(pos_ + n));
    }

    Iterator& operator++() {
      ++pos_;
      return *this;
    }

    Iterator operator++(int) {
      Iterator tmp = *this;
      ++pos_;
      return tmp;
    }

    Iterator& operator--() {
      --pos_;
      return *this;
    }

    Iterator operator--(int) {
      Iterator tmp = *this;
      --pos_;
      return tmp;
    }

    Iterator& operator+=(difference_type n) {
      pos_ += n;
      return *this;
    }

    Iterator& operator-=(difference_type n) {
      pos_ -= n;
      return *this;
    }

    friend bool operator==(Iterator a, Iterator b) { return a.pos_ == b.pos_; }

    friend bool operator!=(Iterator a, Iterator b) { return !(a == b); }

    friend bool operator<(Iterator a, Iterator b) { return a.pos_ < b.pos_; }

    friend bool operator>(Iterator a, Iterator b) { return b < a; }

    friend bool operator<=(Iterator a, Iterator b) { return !(b < a); }

    friend bool operator>=(Iterator a, Iterator b) { return !(a < b); }

    friend difference_type operator-(Iterator a, Iterator b) {
      return a.pos_ - b.pos_;
    }

    friend Iterator operator+(Iterator it, difference_type n) {
      it += n;
      return it;
    }

    friend Iterator operator-(Iterator it, difference_type n) {
      it -= n;
      return it;
    }

   private:
    const EncodedView& encoded_view_;
    difference_type pos_;
  };

  using value_type = size_t;
  using size_type = offset_t;
  using difference_type = ptrdiff_t;
  using const_iterator = Iterator;

  // |image_index| is the annotated image being adapted, and is required to
  // remain valid for the lifetime of the object.
  explicit EncodedView(const ImageIndex& image_index);

  // Projects |location| to a scalar value that describes the content at a
  // higher level of abstraction.
  value_type Projection(offset_t location) const;

  // Returns the cardinality of the projection, i.e., the upper bound on
  // values returned by Projection().
  value_type Cardinality() const;

  const ImageIndex& image_index() const { return image_index_; }

  // Range functions.
  size_type size() const { return size_type(image_index_.size()); }
  const_iterator begin() const {
    return const_iterator{*this, difference_type(0)};
  }
  const_iterator end() const {
    return const_iterator{*this, difference_type(size())};
  }

 private:
  const ImageIndex& image_index_;
};

}  // namespace zucchini

#endif  // CHROME_INSTALLER_ZUCCHINI_ENCODED_VIEW_H_
