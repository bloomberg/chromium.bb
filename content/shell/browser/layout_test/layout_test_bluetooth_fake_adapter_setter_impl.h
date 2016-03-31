// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_BLUETOOTH_FAKE_ADAPTER_SETTER_IMPL_H_
#define CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_BLUETOOTH_FAKE_ADAPTER_SETTER_IMPL_H_

#include "base/macros.h"
#include "content/shell/common/layout_test/layout_test_bluetooth_fake_adapter_setter.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {
class RenderProcessHost;

class LayoutTestBluetoothFakeAdapterSetterImpl
    : public mojom::LayoutTestBluetoothFakeAdapterSetter {
 public:
  static void Create(
      int render_process_id,
      mojom::LayoutTestBluetoothFakeAdapterSetterRequest request);

 private:
  LayoutTestBluetoothFakeAdapterSetterImpl(
      int render_process_id,
      mojom::LayoutTestBluetoothFakeAdapterSetterRequest request);
  ~LayoutTestBluetoothFakeAdapterSetterImpl() override;

  void Set(const mojo::String& adapter_name,
           const SetCallback& callback) override;

  int render_process_id_;

  mojo::StrongBinding<LayoutTestBluetoothFakeAdapterSetter> binding_;

  DISALLOW_COPY_AND_ASSIGN(LayoutTestBluetoothFakeAdapterSetterImpl);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_BLUETOOTH_FAKE_ADAPTER_SETTER_IMPL_H_
