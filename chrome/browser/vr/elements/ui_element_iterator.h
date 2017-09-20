// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_ITERATOR_H_
#define CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_ITERATOR_H_

#include <vector>

namespace vr {

class UiElement;

// The UiElementIterator traverses a UiElement subtree. Do not use this class
// directly. You should, instead, use UiElement::end/begin/rend/rbegin. NB: you
// may find base::Reversed handy if you're doing reverse iteration.
template <typename T, typename U>
class UiElementIterator {
 public:
  explicit UiElementIterator(T* root) {
    current_ = U::Increment(&index_stack_, root, true);
  }
  ~UiElementIterator() {}

  void operator++() { current_ = U::Increment(&index_stack_, current_, false); }
  void operator++(int) {
    current_ = U::Increment(&index_stack_, current_, false);
  }
  T& operator*() { return *current_; }
  bool operator!=(const UiElementIterator& rhs) {
    return current_ != rhs.current_ ||
           index_stack_.empty() != rhs.index_stack_.empty();
  }

 private:
  T* current_ = nullptr;

  // The iterator maintains a stack of indices which represent the current index
  // in each list of children along the ancestor path.
  std::vector<size_t> index_stack_;
};

template <typename T>
struct ForwardIncrementer {
  static T* Increment(std::vector<size_t>* index_stack, T* e, bool init) {
    if (!e || init)
      return e;

    if (!e->children().empty()) {
      index_stack->push_back(0lu);
      return e->children().front().get();
    }

    while (e->parent() && !index_stack->empty() &&
           index_stack->back() + 1lu >= e->parent()->children().size()) {
      index_stack->pop_back();
      e = e->parent();
    }

    if (!e->parent() || index_stack->empty())
      return nullptr;

    index_stack->back()++;
    return e->parent()->children()[index_stack->back()].get();
  }
};

template <typename T>
struct ReverseIncrementer {
  static T* Increment(std::vector<size_t>* index_stack, T* e, bool init) {
    if (!e || (index_stack->empty() && !init))
      return nullptr;

    bool should_descend = false;
    if (index_stack->empty()) {
      should_descend = true;
    } else if (e->parent() && index_stack->back() > 0lu) {
      index_stack->back()--;
      e = e->parent()->children()[index_stack->back()].get();
      should_descend = true;
    }

    if (should_descend) {
      while (!e->children().empty()) {
        index_stack->push_back(e->children().size() - 1lu);
        e = e->children().back().get();
      }
      return e;
    }

    index_stack->pop_back();
    return e->parent();
  }
};

typedef UiElementIterator<UiElement, ForwardIncrementer<UiElement>>
    ForwardUiElementIterator;
typedef UiElementIterator<const UiElement, ForwardIncrementer<const UiElement>>
    ConstForwardUiElementIterator;
typedef UiElementIterator<UiElement, ReverseIncrementer<UiElement>>
    ReverseUiElementIterator;
typedef UiElementIterator<const UiElement, ReverseIncrementer<const UiElement>>
    ConstReverseUiElementIterator;

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_ITERATOR_H_
