// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_port_type_converters.h"

namespace mojo {

// static
content::TransferredMessagePort
TypeConverter<content::TransferredMessagePort,
              content::MojoTransferredMessagePortPtr>::
    Convert(const content::MojoTransferredMessagePortPtr& input) {
  content::TransferredMessagePort output;
  output.id = input->id;
  output.send_messages_as_values = input->send_messages_as_values;
  return output;
}

// static
content::MojoTransferredMessagePortPtr
TypeConverter<content::MojoTransferredMessagePortPtr,
              content::TransferredMessagePort>::
    Convert(const content::TransferredMessagePort& input) {
  content::MojoTransferredMessagePortPtr output(
      content::MojoTransferredMessagePort::New());
  output->id = input.id;
  output->send_messages_as_values = input.send_messages_as_values;
  return output;
}

}  // namespace mojo
