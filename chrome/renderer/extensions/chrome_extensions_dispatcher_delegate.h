// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_CHROME_EXTENSIONS_DISPATCHER_DELEGATE_H_
#define CHROME_RENDERER_EXTENSIONS_CHROME_EXTENSIONS_DISPATCHER_DELEGATE_H_

#include "extensions/renderer/dispatcher_delegate.h"

class ChromeExtensionsDispatcherDelegate
    : public extensions::DispatcherDelegate {
 public:
  ChromeExtensionsDispatcherDelegate();
  ~ChromeExtensionsDispatcherDelegate() override;

 private:
  // extensions::DispatcherDelegate implementation.
  scoped_ptr<extensions::ScriptContext> CreateScriptContext(
      const v8::Handle<v8::Context>& v8_context,
      blink::WebFrame* frame,
      const extensions::Extension* extension,
      extensions::Feature::Context context_type,
      const extensions::Extension* effective_extension,
      extensions::Feature::Context effective_context_type) override;
  void InitOriginPermissions(const extensions::Extension* extension,
                             bool is_extension_active) override;
  void RegisterNativeHandlers(extensions::Dispatcher* dispatcher,
                              extensions::ModuleSystem* module_system,
                              extensions::ScriptContext* context) override;
  void PopulateSourceMap(
      extensions::ResourceBundleSourceMap* source_map) override;
  void RequireAdditionalModules(extensions::ScriptContext* context,
                                bool is_within_platform_app) override;
  void OnActiveExtensionsUpdated(
      const std::set<std::string>& extensions_ids) override;
  void SetChannel(int channel) override;

  DISALLOW_COPY_AND_ASSIGN(ChromeExtensionsDispatcherDelegate);
};

#endif  // CHROME_RENDERER_EXTENSIONS_CHROME_EXTENSIONS_DISPATCHER_DELEGATE_H_
