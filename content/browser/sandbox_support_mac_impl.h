// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SANDBOX_SUPPORT_MAC_IMPL_H_
#define CONTENT_BROWSER_SANDBOX_SUPPORT_MAC_IMPL_H_

#include "base/macros.h"
#include "content/common/sandbox_support_mac.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace service_manager {
struct BindSourceInfo;
}

namespace content {

// Performs privileged operations on behalf of sandboxed child processes.
// This is used to implement the blink::WebSandboxSupport interface in the
// renderer. However all child process types have access to this interface.
// This class lives on the IO thread and is owned by the Mojo interface
// registry.
class SandboxSupportMacImpl : public mojom::SandboxSupportMac {
 public:
  SandboxSupportMacImpl();
  ~SandboxSupportMacImpl() override;

  void BindRequest(mojom::SandboxSupportMacRequest request,
                   const service_manager::BindSourceInfo& source_info);

  // content::mojom::SandboxSupportMac:
  void GetSystemColors(GetSystemColorsCallback callback) override;
  void LoadFont(const base::string16& font_name,
                float font_point_size,
                LoadFontCallback callback) override;

 private:
  mojo::BindingSet<mojom::SandboxSupportMac> bindings_;

  DISALLOW_COPY_AND_ASSIGN(SandboxSupportMacImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SANDBOX_SUPPORT_MAC_IMPL_H_
