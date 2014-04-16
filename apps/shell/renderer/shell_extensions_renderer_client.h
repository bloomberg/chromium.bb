// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHELL_RENDERER_SHELL_EXTENSIONS_RENDERER_CLIENT_H_
#define APPS_SHELL_RENDERER_SHELL_EXTENSIONS_RENDERER_CLIENT_H_

#include "base/macros.h"
#include "extensions/renderer/extensions_renderer_client.h"

namespace apps {

class ShellExtensionsRendererClient
    : public extensions::ExtensionsRendererClient {
 public:
  ShellExtensionsRendererClient();
  virtual ~ShellExtensionsRendererClient();

  // extensions::ExtensionsRendererClient implementation.
  virtual bool IsIncognitoProcess() const OVERRIDE;
  virtual int GetLowestIsolatedWorldId() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellExtensionsRendererClient);
};

}  // namespace apps

#endif  // APPS_SHELL_RENDERER_SHELL_EXTENSIONS_RENDERER_CLIENT_H_
