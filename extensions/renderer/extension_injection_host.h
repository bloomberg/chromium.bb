// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EXTENSION_INJECTION_HOST_H_
#define EXTENSIONS_RENDERER_EXTENSION_INJECTION_HOST_H_

#include "base/memory/ref_counted.h"
#include "extensions/common/extension.h"
#include "extensions/renderer/injection_host.h"

namespace extensions {
class ExtensionSet;

// A wrapper class that holds an extension and implements the InjectionHost
// interface.
class ExtensionInjectionHost : public InjectionHost {
 public:
  ExtensionInjectionHost(const Extension* extension);
  ~ExtensionInjectionHost() override;

  // Create an ExtensionInjectionHost object. If the extension is gone, returns
  // a null scoped ptr.
  static scoped_ptr<const ExtensionInjectionHost> Create(
      const std::string& extension_id, const ExtensionSet* extensions);

 private:
  // InjectionHost:
  std::string GetContentSecurityPolicy() const override;
  const GURL& url() const override;
  const std::string& name() const override;
  PermissionsData::AccessType CanExecuteOnFrame(
      const GURL& document_url,
      const GURL& top_frame_url,
      int tab_id,
      bool is_declarative) const override;
  bool ShouldNotifyBrowserOfInjection() const override;

  const Extension* extension_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInjectionHost);
};

}  // namespace extesions

#endif  // EXTENSIONS_RENDERER_EXTENSION_INJECTION_HOST_H_
