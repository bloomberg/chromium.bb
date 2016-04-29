// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_GPU_GPU_TYPE_CONVERTERS_H_
#define COMPONENTS_MUS_GPU_GPU_TYPE_CONVERTERS_H_

#include "components/mus/public/interfaces/channel_handle.mojom.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace IPC {
struct ChannelHandle;
}

namespace mojo {

template <>
struct TypeConverter<mus::mojom::ChannelHandlePtr, IPC::ChannelHandle> {
  static mus::mojom::ChannelHandlePtr Convert(const IPC::ChannelHandle& handle);
};

template <>
struct TypeConverter<IPC::ChannelHandle, mus::mojom::ChannelHandlePtr> {
  static IPC::ChannelHandle Convert(const mus::mojom::ChannelHandlePtr& handle);
};

}  // namespace mojo

#endif  // COMPONENTS_MUS_GPU_GPU_TYPE_CONVERTERS_H_
