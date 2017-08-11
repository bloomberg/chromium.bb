// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_extensions_api_client.h"

#include "base/memory/ptr_util.h"
#include "extensions/browser/api/messaging/messaging_delegate.h"
#include "extensions/shell/browser/delegates/shell_kiosk_delegate.h"
#include "extensions/shell/browser/shell_app_view_guest_delegate.h"
#include "extensions/shell/browser/shell_extension_web_contents_observer.h"
#include "extensions/shell/browser/shell_virtual_keyboard_delegate.h"

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "extensions/shell/browser/api/file_system/shell_file_system_delegate.h"
#endif

namespace extensions {

ShellExtensionsAPIClient::ShellExtensionsAPIClient() {}

ShellExtensionsAPIClient::~ShellExtensionsAPIClient() {}

void ShellExtensionsAPIClient::AttachWebContentsHelpers(
    content::WebContents* web_contents) const {
  ShellExtensionWebContentsObserver::CreateForWebContents(web_contents);
}

AppViewGuestDelegate* ShellExtensionsAPIClient::CreateAppViewGuestDelegate()
    const {
  return new ShellAppViewGuestDelegate();
}

std::unique_ptr<VirtualKeyboardDelegate>
ShellExtensionsAPIClient::CreateVirtualKeyboardDelegate() const {
  return base::MakeUnique<ShellVirtualKeyboardDelegate>();
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
FileSystemDelegate* ShellExtensionsAPIClient::GetFileSystemDelegate() {
  if (!file_system_delegate_)
    file_system_delegate_ = base::MakeUnique<ShellFileSystemDelegate>();
  return file_system_delegate_.get();
}
#endif

MessagingDelegate* ShellExtensionsAPIClient::GetMessagingDelegate() {
  // The default implementation does nothing, which is fine.
  if (!messaging_delegate_)
    messaging_delegate_ = base::MakeUnique<MessagingDelegate>();
  return messaging_delegate_.get();
}

FeedbackPrivateDelegate*
ShellExtensionsAPIClient::GetFeedbackPrivateDelegate() {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace extensions
