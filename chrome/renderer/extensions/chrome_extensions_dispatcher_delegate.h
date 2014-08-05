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

  bool WasWebRequestUsedBySomeExtensions() const { return webrequest_used_; }

 private:
  // extensions::DispatcherDelegate implementation.
  virtual scoped_ptr<extensions::ScriptContext> CreateScriptContext(
      const v8::Handle<v8::Context>& v8_context,
      blink::WebFrame* frame,
      const extensions::Extension* extension,
      extensions::Feature::Context context_type) OVERRIDE;
  virtual void InitOriginPermissions(const extensions::Extension* extension,
                                     bool is_extension_active) OVERRIDE;
  virtual void RegisterNativeHandlers(
      extensions::Dispatcher* dispatcher,
      extensions::ModuleSystem* module_system,
      extensions::ScriptContext* context) OVERRIDE;
  virtual void PopulateSourceMap(
      extensions::ResourceBundleSourceMap* source_map) OVERRIDE;
  virtual void RequireAdditionalModules(extensions::ScriptContext* context,
                                        bool is_within_platform_app) OVERRIDE;
  virtual void OnActiveExtensionsUpdated(
      const std::set<std::string>& extensions_ids) OVERRIDE;
  virtual void SetChannel(int channel) OVERRIDE;
  virtual void ClearTabSpecificPermissions(
      const extensions::Dispatcher* dispatcher,
      int tab_id,
      const std::vector<std::string>& extension_ids) OVERRIDE;
  virtual void UpdateTabSpecificPermissions(
      const extensions::Dispatcher* dispatcher,
      const GURL& url,
      int tab_id,
      const std::string& extension_id,
      const extensions::URLPatternSet& origin_set) OVERRIDE;
  virtual void HandleWebRequestAPIUsage(bool webrequest_used) OVERRIDE;

  // Status of webrequest usage.
  bool webrequest_used_;

  DISALLOW_COPY_AND_ASSIGN(ChromeExtensionsDispatcherDelegate);
};

#endif  // CHROME_RENDERER_EXTENSIONS_CHROME_EXTENSIONS_DISPATCHER_DELEGATE_H_
