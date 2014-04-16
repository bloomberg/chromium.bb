// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_CHROME_EXTENSIONS_RENDERER_CLIENT_H_
#define CHROME_RENDERER_EXTENSIONS_CHROME_EXTENSIONS_RENDERER_CLIENT_H_

#include "base/macros.h"
#include "extensions/renderer/extensions_renderer_client.h"

class ChromeExtensionsRendererClient
    : public extensions::ExtensionsRendererClient {
 public:
  ChromeExtensionsRendererClient();
  virtual ~ChromeExtensionsRendererClient();

  // Get the LazyInstance for ChromeExtensionsRendererClient.
  static ChromeExtensionsRendererClient* GetInstance();

  // extensions::ExtensionsRendererClient implementation.
  virtual bool IsIncognitoProcess() const OVERRIDE;
  virtual int GetLowestIsolatedWorldId() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeExtensionsRendererClient);
};

#endif  // CHROME_RENDERER_EXTENSIONS_CHROME_EXTENSIONS_RENDERER_CLIENT_H_
