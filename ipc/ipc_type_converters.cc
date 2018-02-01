// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_type_converters.h"
#include "mojo/public/interfaces/bindings/native_struct.mojom.h"

namespace mojo {

// IPC::mojom::SerializedHandle is intended to be a duplicate of
// native::SerializedHandle. Assert that their values are the
// same.
static_assert(
    static_cast<int32_t>(IPC::mojom::SerializedHandle_Type::MOJO_HANDLE) ==
        static_cast<int32_t>(native::SerializedHandle_Type::MOJO_HANDLE),
    "Types in IPC::mojom::SerializedHandle should exactly match those in "
    "native::SerializedHandle.");
static_assert(
    static_cast<int32_t>(IPC::mojom::SerializedHandle_Type::PLATFORM_FILE) ==
        static_cast<int32_t>(native::SerializedHandle_Type::PLATFORM_FILE),
    "Types in IPC::mojom::SerializedHandle should exactly match those in "
    "native::SerializedHandle.");
static_assert(
    static_cast<int32_t>(IPC::mojom::SerializedHandle_Type::WIN_HANDLE) ==
        static_cast<int32_t>(native::SerializedHandle_Type::WIN_HANDLE),
    "Types in IPC::mojom::SerializedHandle should exactly match those in "
    "native::SerializedHandle.");
static_assert(
    static_cast<int32_t>(IPC::mojom::SerializedHandle_Type::MACH_PORT) ==
        static_cast<int32_t>(native::SerializedHandle_Type::MACH_PORT),
    "Types in IPC::mojom::SerializedHandle should exactly match those in "
    "native::SerializedHandle.");
static_assert(
    static_cast<int32_t>(IPC::mojom::SerializedHandle_Type::FUCHSIA_HANDLE) ==
        static_cast<int32_t>(native::SerializedHandle_Type::FUCHSIA_HANDLE),
    "Types in IPC::mojom::SerializedHandle should exactly match those in "
    "native::SerializedHandle.");

// static
IPC::MessageAttachment::Type
TypeConverter<IPC::MessageAttachment::Type, IPC::mojom::SerializedHandle_Type>::
    Convert(IPC::mojom::SerializedHandle_Type type) {
  switch (type) {
    case IPC::mojom::SerializedHandle_Type::MOJO_HANDLE:
      return IPC::MessageAttachment::Type::MOJO_HANDLE;
    case IPC::mojom::SerializedHandle_Type::PLATFORM_FILE:
      return IPC::MessageAttachment::Type::PLATFORM_FILE;
    case IPC::mojom::SerializedHandle_Type::WIN_HANDLE:
      return IPC::MessageAttachment::Type::WIN_HANDLE;
    case IPC::mojom::SerializedHandle_Type::MACH_PORT:
      return IPC::MessageAttachment::Type::MACH_PORT;
    case IPC::mojom::SerializedHandle_Type::FUCHSIA_HANDLE:
      return IPC::MessageAttachment::Type::FUCHSIA_HANDLE;
  }
  NOTREACHED();
  return IPC::MessageAttachment::Type::MOJO_HANDLE;
}

// static
IPC::mojom::SerializedHandle_Type TypeConverter<
    IPC::mojom::SerializedHandle_Type,
    IPC::MessageAttachment::Type>::Convert(IPC::MessageAttachment::Type type) {
  switch (type) {
    case IPC::MessageAttachment::Type::MOJO_HANDLE:
      return IPC::mojom::SerializedHandle_Type::MOJO_HANDLE;
    case IPC::MessageAttachment::Type::PLATFORM_FILE:
      return IPC::mojom::SerializedHandle_Type::PLATFORM_FILE;
    case IPC::MessageAttachment::Type::WIN_HANDLE:
      return IPC::mojom::SerializedHandle_Type::WIN_HANDLE;
    case IPC::MessageAttachment::Type::MACH_PORT:
      return IPC::mojom::SerializedHandle_Type::MACH_PORT;
    case IPC::MessageAttachment::Type::FUCHSIA_HANDLE:
      return IPC::mojom::SerializedHandle_Type::FUCHSIA_HANDLE;
  }
  NOTREACHED();
  return IPC::mojom::SerializedHandle_Type::MOJO_HANDLE;
}

}  // namespace mojo
