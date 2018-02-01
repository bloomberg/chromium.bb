// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_CONVERTER_H_
#define IPC_CONVERTER_H_

#include "ipc/ipc.mojom.h"
#include "ipc/ipc_message_attachment.h"
#include "mojo/public/cpp/bindings/type_converter.h"  // nogncheck

namespace mojo {

template <>
struct TypeConverter<IPC::MessageAttachment::Type,
                     IPC::mojom::SerializedHandle_Type> {
  static IPC::MessageAttachment::Type Convert(
      IPC::mojom::SerializedHandle_Type type);
};

template <>
struct TypeConverter<IPC::mojom::SerializedHandle_Type,
                     IPC::MessageAttachment::Type> {
  static IPC::mojom::SerializedHandle_Type Convert(
      IPC::MessageAttachment::Type type);
};

}  // namespace mojo

#endif  // IPC_CONVERTER_H
