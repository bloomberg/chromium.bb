// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EXTENSIONS_RENDERER_CLIENT_H_
#define EXTENSIONS_RENDERER_EXTENSIONS_RENDERER_CLIENT_H_

class ResourceBundleSourceMap;

namespace extensions {

class ModuleSystem;
class ScriptContext;

// Interface to allow the extensions module to make render-process-specific
// queries of the embedder. Should be Set() once in the render process.
//
// NOTE: Methods that do not require knowledge of renderer concepts should be
// added in ExtensionsClient (extensions/common/extensions_client.h) even if
// they are only used in the renderer process.
class ExtensionsRendererClient {
 public:
  virtual ~ExtensionsRendererClient() {}

  // Returns true if the current render process was launched incognito.
  virtual bool IsIncognitoProcess() const = 0;

  // Returns the lowest isolated world ID available to extensions.
  // Must be greater than 0. See blink::WebFrame::executeScriptInIsolatedWorld
  // (third_party/WebKit/public/web/WebFrame.h) for additional context.
  virtual int GetLowestIsolatedWorldId() const = 0;

  // Registers additional native C++ code handlers for JS API functions.
  virtual void RegisterNativeHandlers(ModuleSystem* module_system,
                                      ScriptContext* context) = 0;

  // Registers additional JS source code resources for API implementations.
  virtual void PopulateSourceMap(ResourceBundleSourceMap* source_map) = 0;

  // Returns the single instance of |this|.
  static ExtensionsRendererClient* Get();

  // Initialize the single instance.
  static void Set(ExtensionsRendererClient* client);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_EXTENSIONS_RENDERER_CLIENT_H_
