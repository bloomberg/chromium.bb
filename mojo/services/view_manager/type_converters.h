// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_TYPE_CONVERTERS_H_
#define MOJO_SERVICES_VIEW_MANAGER_TYPE_CONVERTERS_H_

#include "mojo/public/cpp/bindings/type_converter.h"

namespace mojo {

class Buffer;

namespace view_manager {

class INode;

namespace service {
class Node;
}  // namespace service
}  // namespace view_manager

template <>
class TypeConverter<view_manager::INode,
                    view_manager::service::Node*> {
 public:
  static view_manager::INode ConvertFrom(
      const view_manager::service::Node* node,
      Buffer* buf);

  MOJO_ALLOW_IMPLICIT_TYPE_CONVERSION();
};

}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_TYPE_CONVERTERS_H_
