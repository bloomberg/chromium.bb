// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/content_client/shell_content_browser_client.h"

#include "ash/shell/content_client/shell_browser_main_parts.h"
#include "content/public/common/content_descriptors.h"
#include "content/shell/browser/shell_browser_context.h"
#include "gin/v8_initializer.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace ash {
namespace shell {

ShellContentBrowserClient::ShellContentBrowserClient()
    :
#if defined(OS_POSIX) && !defined(OS_MACOSX)
      v8_natives_fd_(-1),
      v8_snapshot_fd_(-1),
#endif  // OS_POSIX && !OS_MACOSX
      shell_browser_main_parts_(nullptr) {
}

ShellContentBrowserClient::~ShellContentBrowserClient() {
}

content::BrowserMainParts* ShellContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  shell_browser_main_parts_ =  new ShellBrowserMainParts(parameters);
  return shell_browser_main_parts_;
}

net::URLRequestContextGetter* ShellContentBrowserClient::CreateRequestContext(
    content::BrowserContext* content_browser_context,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  content::ShellBrowserContext* shell_context =
      static_cast<content::ShellBrowserContext*>(content_browser_context);
  return shell_context->CreateRequestContext(protocol_handlers,
                                             request_interceptors.Pass());
}

#if defined(OS_POSIX) && !defined(OS_MACOSX)
void ShellContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    content::FileDescriptorInfo* mappings) {
#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
  if (v8_natives_fd_.get() == -1 || v8_snapshot_fd_.get() == -1) {
    int v8_natives_fd = -1;
    int v8_snapshot_fd = -1;
    if (gin::V8Initializer::OpenV8FilesForChildProcesses(&v8_natives_fd,
                                                         &v8_snapshot_fd)) {
      v8_natives_fd_.reset(v8_natives_fd);
      v8_snapshot_fd_.reset(v8_snapshot_fd);
    }
  }
  DCHECK(v8_natives_fd_.get() != -1 && v8_snapshot_fd_.get() != -1);
  mappings->Share(kV8NativesDataDescriptor, v8_natives_fd_.get());
  mappings->Share(kV8SnapshotDataDescriptor, v8_snapshot_fd_.get());
#endif  // V8_USE_EXTERNAL_STARTUP_DATA
}
#endif  // OS_POSIX && !OS_MACOSX

content::ShellBrowserContext* ShellContentBrowserClient::browser_context() {
  return shell_browser_main_parts_->browser_context();
}

}  // namespace examples
}  // namespace views
