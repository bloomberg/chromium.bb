// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_INL_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_INL_H_
#pragma once

template<typename T>
class FormGroupMatchesByCompareFunctor {
 public:
  explicit FormGroupMatchesByCompareFunctor(const T& form_group)
      : form_group_(form_group) {
  }

  bool operator()(const T* form_group) {
    return form_group->Compare(form_group_) == 0;
  }

  bool operator()(const T& form_group) {
    return form_group.Compare(form_group_) == 0;
  }

 private:
  const T& form_group_;
};

template<typename C, typename T>
bool FindByContents(const C& container, const T& form_group) {
  return std::find_if(
      container.begin(),
      container.end(),
      FormGroupMatchesByCompareFunctor<T>(form_group)) != container.end();
}

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_INL_H_
