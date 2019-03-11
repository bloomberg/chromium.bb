// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/ppapi_plugin/ppapi_blink_platform_impl.h"

#include <stdint.h>

#include <map>

#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "content/child/child_thread_impl.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "third_party/blink/public/platform/web_storage_namespace.h"
#include "third_party/blink/public/platform/web_string.h"

#if defined(OS_MACOSX)
#include "content/child/child_process_sandbox_support_impl_mac.h"
#elif defined(OS_LINUX)
#include "content/child/child_process_sandbox_support_impl_linux.h"
#endif

using blink::WebSandboxSupport;
using blink::WebString;
using blink::WebUChar;
using blink::WebUChar32;

typedef struct CGFont* CGFontRef;

namespace content {

PpapiBlinkPlatformImpl::PpapiBlinkPlatformImpl() {
#if defined(OS_LINUX)
  font_loader_ =
      sk_make_sp<font_service::FontLoader>(ChildThread::Get()->GetConnector());
  SkFontConfigInterface::SetGlobal(font_loader_);
  sandbox_support_.reset(new WebSandboxSupportLinux(font_loader_));
#elif defined(OS_MACOSX)
  sandbox_support_.reset(
      new WebSandboxSupportMac(ChildThread::Get()->GetConnector()));
#endif
}

PpapiBlinkPlatformImpl::~PpapiBlinkPlatformImpl() {
}

void PpapiBlinkPlatformImpl::Shutdown() {
#if defined(OS_LINUX) || defined(OS_MACOSX)
  // SandboxSupport contains a map of OutOfProcessFont objects, which hold
  // WebStrings and WebVectors, which become invalidated when blink is shut
  // down. Hence, we need to clear that map now, just before blink::shutdown()
  // is called.
  sandbox_support_.reset();
#endif
}

blink::WebSandboxSupport* PpapiBlinkPlatformImpl::GetSandboxSupport() {
#if defined(OS_LINUX) || defined(OS_MACOSX)
  return sandbox_support_.get();
#else
  return nullptr;
#endif
}

uint64_t PpapiBlinkPlatformImpl::VisitedLinkHash(const char* canonical_url,
                                                 size_t length) {
  NOTREACHED();
  return 0;
}

bool PpapiBlinkPlatformImpl::IsLinkVisited(uint64_t link_hash) {
  NOTREACHED();
  return false;
}

blink::WebString PpapiBlinkPlatformImpl::DefaultLocale() {
  return blink::WebString::FromUTF8("en");
}

blink::WebThemeEngine* PpapiBlinkPlatformImpl::ThemeEngine() {
  NOTREACHED();
  return nullptr;
}

blink::WebData PpapiBlinkPlatformImpl::GetDataResource(const char* name) {
  NOTREACHED();
  return blink::WebData();
}

std::unique_ptr<blink::WebStorageNamespace>
PpapiBlinkPlatformImpl::CreateLocalStorageNamespace() {
  NOTREACHED();
  return nullptr;
}

int PpapiBlinkPlatformImpl::DatabaseDeleteFile(
    const blink::WebString& vfs_file_name,
    bool sync_dir) {
  NOTREACHED();
  return 0;
}

}  // namespace content
