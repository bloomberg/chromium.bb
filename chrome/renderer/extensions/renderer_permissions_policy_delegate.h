// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_RENDERER_PERMISSIONS_POLICY_DELEGATE_H_
#define CHROME_RENDERER_EXTENSIONS_RENDERER_PERMISSIONS_POLICY_DELEGATE_H_

#include "extensions/common/permissions/permissions_data.h"

namespace extensions {

class Dispatcher;

// Policy delegate for the renderer process.
class RendererPermissionsPolicyDelegate
    : public PermissionsData::PolicyDelegate {
 public:
  explicit RendererPermissionsPolicyDelegate(Dispatcher* dispatcher);
  virtual ~RendererPermissionsPolicyDelegate();

  virtual bool CanExecuteScriptOnPage(const Extension* extension,
                                      const GURL& document_url,
                                      const GURL& top_document_url,
                                      int tab_id,
                                      int process_id,
                                      std::string* error) OVERRIDE;

 private:
  Dispatcher* dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(RendererPermissionsPolicyDelegate);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_RENDERER_PERMISSIONS_POLICY_DELEGATE_H_
