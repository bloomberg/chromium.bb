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
  virtual ~ChromeExtensionsDispatcherDelegate();

 private:
  // extensions::DispatcherDelegate implementation.
  virtual scoped_ptr<extensions::ScriptContext> CreateScriptContext(
      const v8::Handle<v8::Context>& v8_context,
      blink::WebFrame* frame,
      const extensions::Extension* extension,
      extensions::Feature::Context context_type,
      const extensions::Extension* effective_extension,
      extensions::Feature::Context effective_context_type) override;
  virtual void InitOriginPermissions(const extensions::Extension* extension,
                                     bool is_extension_active) override;
  virtual void RegisterNativeHandlers(
      extensions::Dispatcher* dispatcher,
      extensions::ModuleSystem* module_system,
      extensions::ScriptContext* context) override;
  virtual void PopulateSourceMap(
      extensions::ResourceBundleSourceMap* source_map) override;
  virtual void RequireAdditionalModules(extensions::ScriptContext* context,
                                        bool is_within_platform_app) override;
  virtual void OnActiveExtensionsUpdated(
      const std::set<std::string>& extensions_ids) override;
  virtual void SetChannel(int channel) override;
  virtual void ClearTabSpecificPermissions(
      const extensions::Dispatcher* dispatcher,
      int tab_id,
      const std::vector<std::string>& extension_ids) override;
  virtual void UpdateTabSpecificPermissions(
      const extensions::Dispatcher* dispatcher,
      const GURL& url,
      int tab_id,
      const std::string& extension_id,
      const extensions::URLPatternSet& origin_set) override;

  DISALLOW_COPY_AND_ASSIGN(ChromeExtensionsDispatcherDelegate);
};

#endif  // CHROME_RENDERER_EXTENSIONS_CHROME_EXTENSIONS_DISPATCHER_DELEGATE_H_
