// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_CAPABILITY_FILTER_H_
#define MOJO_SHELL_CAPABILITY_FILTER_H_

#include <map>
#include <set>

#include "mojo/public/cpp/bindings/array.h"

namespace mojo {
// TODO(beng): upstream this into mojo repo, array.h so it can be shared with
//             application_impl.cc.
// A |TypeConverter| that will create an |std::set<E>| containing a copy of
// the contents of an |Array<T>|, using |TypeConverter<E, T>| to copy each
// element. If the input array is null, the output set will be empty.
template <typename E, typename T>
struct TypeConverter <std::set<E>, Array<T>> {
  static std::set<E> Convert(const Array<T>& input) {
    std::set<E> result;
    if (!input.is_null()) {
      for (size_t i = 0; i < input.size(); ++i)
        result.insert(TypeConverter<E, T>::Convert(input[i]));
    }
    return result;
  }
};

template <typename T, typename E>
struct TypeConverter <Array<T>, std::set<E>> {
  static Array<T> Convert(const std::set<E>& input) {
    Array<T> result(0u);
    for (auto i : input)
      result.push_back(TypeConverter<T, E>::Convert(i));
    return result.Pass();
  }
};

namespace shell {

// A set of names of interfaces that may be exposed to an application.
using AllowedInterfaces = std::set<std::string>;
// A map of allowed applications to allowed interface sets. See shell.mojom for
// more details.
using CapabilityFilter = std::map<std::string, AllowedInterfaces>;

// Returns a capability filter that allows an application to connect to any
// other application and any service exposed by other applications.
CapabilityFilter GetPermissiveCapabilityFilter();

}  // namespace shell
}  // namespace mojo


#endif  // MOJO_SHELL_CAPABILITY_FILTER_H_
