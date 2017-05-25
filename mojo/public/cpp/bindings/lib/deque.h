// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_DEQUE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_DEQUE_H_

#include <algorithm>
#include <limits>
#include <vector>

#include "base/logging.h"
#include "base/numerics/safe_math.h"

namespace mojo {
namespace test {
template <typename T>
class DequeAccessor;
}

namespace internal {

// This class uses a std::vector as ring buffer, which is expanded when
// necessary.
// The reason to introduce this class is that std::deque is memory inefficient.
// (Please see crbug.com/674287 for more details.)
template <typename T>
class deque {
 public:
  static constexpr size_t kInvalidIndex = std::numeric_limits<size_t>::max();
  static constexpr size_t kDefaultInitialCapacity = 4;

  explicit deque(size_t initial_capacity = kDefaultInitialCapacity)
      : buffer_(initial_capacity) {}

  deque(deque&& other)
      : front_(other.front_),
        back_(other.back_),
        buffer_(std::move(other.buffer_)) {
    other.front_ = kInvalidIndex;
    other.back_ = kInvalidIndex;
  }

  deque& operator=(deque&& other) {
    front_ = other.front_;
    back_ = other.back_;
    buffer_ = std::move(other.buffer_);

    other.front_ = kInvalidIndex;
    other.back_ = kInvalidIndex;

    return *this;
  }

  void push_front(const T& value) {
    expand_if_necessary();
    if (empty()) {
      front_ = 0;
      back_ = 0;
    } else {
      front_ = get_prev(front_);
    }
    buffer_[front_] = value;
  }

  void push_front(T&& value) {
    expand_if_necessary();
    if (empty()) {
      front_ = 0;
      back_ = 0;
    } else {
      front_ = get_prev(front_);
    }
    buffer_[front_] = std::forward<T>(value);
  }

  void pop_front() {
    DCHECK(!empty());

    buffer_[front_] = T();
    if (front_ == back_) {
      front_ = kInvalidIndex;
      back_ = kInvalidIndex;
    } else {
      front_ = get_next(front_);
    }
  }

  T& front() {
    DCHECK(!empty());
    return buffer_[front_];
  }

  const T& front() const {
    DCHECK(!empty());
    return buffer_[front_];
  }

  void push_back(const T& value) {
    expand_if_necessary();
    if (empty()) {
      front_ = 0;
      back_ = 0;
    } else {
      back_ = get_next(back_);
    }
    buffer_[back_] = value;
  }

  void push_back(T&& value) {
    expand_if_necessary();
    if (empty()) {
      front_ = 0;
      back_ = 0;
    } else {
      back_ = get_next(back_);
    }
    buffer_[back_] = std::forward<T>(value);
  }

  void pop_back() {
    DCHECK(!empty());

    buffer_[back_] = T();
    if (front_ == back_) {
      front_ = kInvalidIndex;
      back_ = kInvalidIndex;
    } else {
      back_ = get_prev(back_);
    }
  }

  T& back() {
    DCHECK(!empty());
    return buffer_[back_];
  }

  const T& back() const {
    DCHECK(!empty());
    return buffer_[back_];
  }

  void clear() {
    if (empty())
      return;

    if (back_ >= front_) {
      for (size_t i = front_; i <= back_; ++i)
        buffer_[i] = T();
    } else {
      for (size_t i = front_; i < buffer_.size(); ++i)
        buffer_[i] = T();
      for (size_t i = 0; i <= back_; ++i)
        buffer_[i] = T();
    }

    front_ = kInvalidIndex;
    back_ = kInvalidIndex;
  }

  bool empty() const { return front_ == kInvalidIndex; }

 private:
  friend class ::mojo::test::DequeAccessor<T>;

  size_t get_next(size_t index) {
    DCHECK(!buffer_.empty());
    return index != buffer_.size() - 1 ? index + 1 : 0;
  }

  size_t get_prev(size_t index) {
    DCHECK(!buffer_.empty());
    return index != 0 ? index - 1 : buffer_.size() - 1;
  }

  void expand_if_necessary() {
    if (buffer_.empty()) {
      DCHECK_EQ(kInvalidIndex, front_);
      DCHECK_EQ(kInvalidIndex, back_);

      buffer_.resize(kDefaultInitialCapacity);
    } else {
      // Whether the buffer still has available slots.
      if (empty() || get_prev(front_) != back_)
        return;

      size_t original_size = buffer_.size();
      base::CheckedNumeric<size_t> new_size = original_size;
      new_size *= 2;
      buffer_.resize(new_size.ValueOrDie());

      // Whether we need to move part of the elements to remain a ring buffer.
      if (front_ != 0) {
        if (back_ < original_size / 2) {
          // Move [0, back_] to [original_size, original_size + back_].
          std::move(buffer_.begin(), buffer_.begin() + back_ + 1,
                    buffer_.begin() + original_size);
          back_ += original_size;
        } else {
          // Move [front_, original_size - 1] to
          // [front_ + buffer_.size() - original_size, buffer_.size() - 1].
          size_t new_front = front_ + buffer_.size() - original_size;
          std::move(buffer_.begin() + front_, buffer_.begin() + original_size,
                    buffer_.begin() + new_front);
          front_ = new_front;
        }
      }
    }
  }

  size_t front_ = kInvalidIndex;
  // NOTE: |back_| is inclusive.
  size_t back_ = kInvalidIndex;
  // TODO(yzshen): This buffer default-constructs T instances for unoccupied
  // slots. It would be nice to avoid that.
  std::vector<T> buffer_;
};

template <typename T>
constexpr size_t deque<T>::kInvalidIndex;

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_DEQUE_H_
