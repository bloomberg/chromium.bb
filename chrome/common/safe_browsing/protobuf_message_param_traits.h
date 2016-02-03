// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SAFE_BROWSING_PROTOBUF_MESSAGE_PARAM_TRAITS_H_
#define CHROME_COMMON_SAFE_BROWSING_PROTOBUF_MESSAGE_PARAM_TRAITS_H_

#include <limits.h>

#include <string>

#include "base/pickle.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_param_traits.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

namespace IPC {

template <class Element>
struct ParamTraits<google::protobuf::RepeatedPtrField<Element>> {
  typedef google::protobuf::RepeatedPtrField<Element> param_type;

  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, p.size());
    for (const auto& element : p)
      WriteParam(m, element);
  }

  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p) {
    int size;
    if (!iter->ReadLength(&size) || size < 0)
      return false;
    if (INT_MAX / static_cast<int>(sizeof(void*)) <= size)
      return false;
    p->Clear();
    p->Reserve(size);
    while (size--) {
      if (!ReadParam(m, iter, p->Add()))
        return false;
    }
    return true;
  }

  static void Log(const param_type& p, std::string* l) {
    bool logged_first = false;
    for (const auto& element : p) {
      if (logged_first)
        l->push_back(' ');
      else
        logged_first = true;
      LogParam(element, l);
    }
  }
};

}  // namespace IPC

#endif  // CHROME_COMMON_SAFE_BROWSING_PROTOBUF_MESSAGE_PARAM_TRAITS_H_
