// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_PORT_TYPE_CONVERTERS_H_
#define CONTENT_COMMON_SERVICE_PORT_TYPE_CONVERTERS_H_

#include "content/common/content_export.h"
#include "content/common/service_port_service.mojom.h"
#include "content/public/common/message_port_types.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace mojo {

template <>
struct CONTENT_EXPORT TypeConverter<content::TransferredMessagePort,
                                    content::MojoTransferredMessagePortPtr> {
  static content::TransferredMessagePort Convert(
      const content::MojoTransferredMessagePortPtr& input);
};

template <>
struct CONTENT_EXPORT TypeConverter<content::MojoTransferredMessagePortPtr,
                                    content::TransferredMessagePort> {
  static content::MojoTransferredMessagePortPtr Convert(
      const content::TransferredMessagePort& input);
};

}  // namespace mojo

#endif  // CONTENT_COMMON_SERVICE_PORT_TYPE_CONVERTERS_H_
