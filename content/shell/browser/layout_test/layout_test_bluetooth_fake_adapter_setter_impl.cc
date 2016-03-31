// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_bluetooth_fake_adapter_setter_impl.h"

#include <string>

#include "content/public/test/layouttest_support.h"
#include "content/shell/browser/layout_test/layout_test_bluetooth_adapter_provider.h"

namespace content {

// static
void LayoutTestBluetoothFakeAdapterSetterImpl::Create(
    int render_process_id,
    mojom::LayoutTestBluetoothFakeAdapterSetterRequest request) {
  new LayoutTestBluetoothFakeAdapterSetterImpl(render_process_id,
                                               std::move(request));
}

LayoutTestBluetoothFakeAdapterSetterImpl::
    LayoutTestBluetoothFakeAdapterSetterImpl(
        int render_process_id,
        mojom::LayoutTestBluetoothFakeAdapterSetterRequest request)
    : render_process_id_(render_process_id),
      binding_(this, std::move(request)) {}

LayoutTestBluetoothFakeAdapterSetterImpl::
    ~LayoutTestBluetoothFakeAdapterSetterImpl() {}

void LayoutTestBluetoothFakeAdapterSetterImpl::Set(
    const mojo::String& adapter_name,
    const SetCallback& callback) {
  SetBluetoothAdapter(render_process_id_,
                      LayoutTestBluetoothAdapterProvider::GetBluetoothAdapter(
                          adapter_name));
  callback.Run();
}

}  // namespace content
