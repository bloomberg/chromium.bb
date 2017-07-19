// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSIONS_API_CLIENT_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSIONS_API_CLIENT_H_

#include <memory>

#include "extensions/browser/api/extensions_api_client.h"

#include "build/build_config.h"

namespace extensions {

class VirtualKeyboardDelegate;

class ShellExtensionsAPIClient : public ExtensionsAPIClient {
 public:
  ShellExtensionsAPIClient();
  ~ShellExtensionsAPIClient() override;

  // ExtensionsAPIClient implementation.
  void AttachWebContentsHelpers(content::WebContents* web_contents) const
      override;
  AppViewGuestDelegate* CreateAppViewGuestDelegate() const override;
  std::unique_ptr<VirtualKeyboardDelegate> CreateVirtualKeyboardDelegate()
      const override;
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  FileSystemDelegate* GetFileSystemDelegate() override;
#endif

 private:
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  std::unique_ptr<FileSystemDelegate> file_system_delegate_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ShellExtensionsAPIClient);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSIONS_API_CLIENT_H_
