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
#include "third_party/blink/public/platform/mac/web_sandbox_support.h"
#elif defined(OS_POSIX) && !defined(OS_ANDROID)
#include "content/child/child_process_sandbox_support_impl_linux.h"
#include "third_party/blink/public/platform/linux/web_fallback_font.h"
#include "third_party/blink/public/platform/linux/web_sandbox_support.h"
#include "third_party/icu/source/common/unicode/utf16.h"
#endif

using blink::WebSandboxSupport;
using blink::WebString;
using blink::WebUChar;
using blink::WebUChar32;

typedef struct CGFont* CGFontRef;

namespace content {

#if !defined(OS_ANDROID) && !defined(OS_WIN)

class PpapiBlinkPlatformImpl::SandboxSupport : public WebSandboxSupport {
 public:
  ~SandboxSupport() override {}

#if defined(OS_MACOSX)
  bool LoadFont(CTFontRef srcFont, CGFontRef* out, uint32_t* fontID) override;
#elif defined(OS_POSIX)
  SandboxSupport();
  void GetFallbackFontForCharacter(
      WebUChar32 character,
      const char* preferred_locale,
      blink::WebFallbackFont* fallbackFont) override;
  void GetWebFontRenderStyleForStrike(const char* family,
                                      int sizeAndStyle,
                                      blink::WebFontRenderStyle* out) override;

 private:
  // WebKit likes to ask us for the correct font family to use for a set of
  // unicode code points. It needs this information frequently so we cache it
  // here.
  std::map<int32_t, blink::WebFallbackFont> unicode_font_families_;
  // For debugging crbug.com/312965
  base::PlatformThreadId creation_thread_;
#endif
};

#if defined(OS_MACOSX)

bool PpapiBlinkPlatformImpl::SandboxSupport::LoadFont(CTFontRef src_font,
                                                      CGFontRef* out,
                                                      uint32_t* font_id) {
  // TODO(brettw) this should do the something similar to what
  // RendererBlinkPlatformImpl does and request that the browser load the font.
  // Note: need to unlock the proxy lock like ensureFontLoaded does.
  NOTIMPLEMENTED();
  return false;
}

#elif defined(OS_POSIX)

PpapiBlinkPlatformImpl::SandboxSupport::SandboxSupport()
    : creation_thread_(base::PlatformThread::CurrentId()) {
}

void PpapiBlinkPlatformImpl::SandboxSupport::GetFallbackFontForCharacter(
    WebUChar32 character,
    const char* preferred_locale,
    blink::WebFallbackFont* fallbackFont) {
  ppapi::ProxyLock::AssertAcquired();
  // For debugging crbug.com/312965
  CHECK_EQ(creation_thread_, base::PlatformThread::CurrentId());
  const std::map<int32_t, blink::WebFallbackFont>::const_iterator iter =
      unicode_font_families_.find(character);
  if (iter != unicode_font_families_.end()) {
    fallbackFont->name = iter->second.name;
    fallbackFont->filename = iter->second.filename;
    fallbackFont->fontconfig_interface_id =
        iter->second.fontconfig_interface_id;
    fallbackFont->ttc_index = iter->second.ttc_index;
    fallbackFont->is_bold = iter->second.is_bold;
    fallbackFont->is_italic = iter->second.is_italic;
    return;
  }

  content::GetFallbackFontForCharacter(character, preferred_locale,
                                       fallbackFont);
  unicode_font_families_.insert(std::make_pair(character, *fallbackFont));
}

void PpapiBlinkPlatformImpl::SandboxSupport::GetWebFontRenderStyleForStrike(
    const char* family,
    int sizeAndStyle,
    blink::WebFontRenderStyle* out) {
  GetRenderStyleForStrike(family, sizeAndStyle, out);
}

#endif

#endif  // !defined(OS_ANDROID) && !defined(OS_WIN)

PpapiBlinkPlatformImpl::PpapiBlinkPlatformImpl() {
#if !defined(OS_ANDROID) && !defined(OS_WIN)
  sandbox_support_.reset(new PpapiBlinkPlatformImpl::SandboxSupport);
#endif
}

PpapiBlinkPlatformImpl::~PpapiBlinkPlatformImpl() {
}

void PpapiBlinkPlatformImpl::Shutdown() {
#if !defined(OS_ANDROID) && !defined(OS_WIN)
  // SandboxSupport contains a map of WebFallbackFont objects, which hold
  // WebStrings and WebVectors, which become invalidated when blink is shut
  // down. Hence, we need to clear that map now, just before blink::shutdown()
  // is called.
  sandbox_support_.reset();
#endif
}

blink::WebThread* PpapiBlinkPlatformImpl::CurrentThread() {
  return BlinkPlatformImpl::CurrentThread();
}

blink::WebClipboard* PpapiBlinkPlatformImpl::Clipboard() {
  NOTREACHED();
  return nullptr;
}

blink::WebFileUtilities* PpapiBlinkPlatformImpl::GetFileUtilities() {
  NOTREACHED();
  return nullptr;
}

blink::WebSandboxSupport* PpapiBlinkPlatformImpl::GetSandboxSupport() {
#if !defined(OS_ANDROID) && !defined(OS_WIN)
  return sandbox_support_.get();
#else
  return nullptr;
#endif
}

bool PpapiBlinkPlatformImpl::sandboxEnabled() {
  return true;  // Assume PPAPI is always sandboxed.
}

unsigned long long PpapiBlinkPlatformImpl::VisitedLinkHash(
    const char* canonical_url,
    size_t length) {
  NOTREACHED();
  return 0;
}

bool PpapiBlinkPlatformImpl::IsLinkVisited(unsigned long long link_hash) {
  NOTREACHED();
  return false;
}

void PpapiBlinkPlatformImpl::setCookies(const blink::WebURL& url,
                                        const blink::WebURL& site_for_cookies,
                                        const blink::WebString& value) {
  NOTREACHED();
}

blink::WebString PpapiBlinkPlatformImpl::cookies(
    const blink::WebURL& url,
    const blink::WebURL& site_for_cookies) {
  NOTREACHED();
  return blink::WebString();
}

blink::WebString PpapiBlinkPlatformImpl::DefaultLocale() {
  return blink::WebString::FromUTF8("en");
}

blink::WebThemeEngine* PpapiBlinkPlatformImpl::ThemeEngine() {
  NOTREACHED();
  return nullptr;
}

void PpapiBlinkPlatformImpl::GetPluginList(
    bool refresh,
    const blink::WebSecurityOrigin& mainFrameOrigin,
    blink::WebPluginListBuilder* builder) {
  NOTREACHED();
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

void PpapiBlinkPlatformImpl::dispatchStorageEvent(
    const blink::WebString& key,
    const blink::WebString& old_value,
    const blink::WebString& new_value,
    const blink::WebString& origin,
    const blink::WebURL& url,
    bool is_local_storage) {
  NOTREACHED();
}

int PpapiBlinkPlatformImpl::DatabaseDeleteFile(
    const blink::WebString& vfs_file_name,
    bool sync_dir) {
  NOTREACHED();
  return 0;
}

}  // namespace content
