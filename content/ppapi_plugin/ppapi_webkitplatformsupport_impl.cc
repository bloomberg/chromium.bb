// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/ppapi_plugin/ppapi_webkitplatformsupport_impl.h"

#include <map>

#include "base/synchronization/lock.h"
#include "base/logging.h"
#include "base/string16.h"
#include "build/build_config.h"
#include "content/common/child_process_messages.h"
#include "content/common/child_thread.h"
#include "ppapi/proxy/plugin_globals.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"

#if defined(OS_WIN)
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/win/WebSandboxSupport.h"
#elif defined(OS_MACOSX)
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/mac/WebSandboxSupport.h"
#elif defined(OS_POSIX)
#if !defined(OS_ANDROID)
#include "content/common/child_process_sandbox_support_impl_linux.h"
#endif
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/linux/WebFontFamily.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/linux/WebSandboxSupport.h"

#endif

using WebKit::WebSandboxSupport;
using WebKit::WebString;
using WebKit::WebUChar;

typedef struct CGFont* CGFontRef;

namespace content {

class PpapiWebKitPlatformSupportImpl::SandboxSupport
    : public WebSandboxSupport {
 public:
  virtual ~SandboxSupport() {}

#if defined(OS_WIN)
  virtual bool ensureFontLoaded(HFONT);
#elif defined(OS_MACOSX)
  virtual bool loadFont(
      NSFont* srcFont, CGFontRef* out, uint32_t* fontID);
#elif defined(OS_POSIX)
  virtual void getFontFamilyForCharacters(
      const WebUChar* characters,
      size_t numCharacters,
      const char* preferred_locale,
      WebKit::WebFontFamily* family);
  virtual void getRenderStyleForStrike(
      const char* family, int sizeAndStyle, WebKit::WebFontRenderStyle* out);

 private:
  // WebKit likes to ask us for the correct font family to use for a set of
  // unicode code points. It needs this information frequently so we cache it
  // here. The key in this map is an array of 16-bit UTF16 values from WebKit.
  // The value is a string containing the correct font family.
  base::Lock unicode_font_families_mutex_;
  std::map<string16, WebKit::WebFontFamily> unicode_font_families_;
#endif
};

#if defined(OS_WIN)

bool PpapiWebKitPlatformSupportImpl::SandboxSupport::ensureFontLoaded(
    HFONT font) {
  LOGFONT logfont;
  GetObject(font, sizeof(LOGFONT), &logfont);

  // Use the proxy sender rather than going directly to the ChildThread since
  // the proxy browser sender will properly unlock during sync messages.
  return ppapi::proxy::PluginGlobals::Get()->GetBrowserSender()->Send(
      new ChildProcessHostMsg_PreCacheFont(logfont));
}

#elif defined(OS_MACOSX)

bool PpapiWebKitPlatformSupportImpl::SandboxSupport::loadFont(
    NSFont* src_font,
    CGFontRef* out,
    uint32_t* font_id) {
  // TODO(brettw) this should do the something similar to what
  // RendererWebKitClientImpl does and request that the browser load the font.
  // Note: need to unlock the proxy lock like ensureFontLoaded does.
  NOTIMPLEMENTED();
  return false;
}

#elif defined(OS_ANDROID)

// TODO(jrg): resolve (and implement?) PPAPI SandboxSupport for Android.

void
PpapiWebKitPlatformSupportImpl::SandboxSupport::getFontFamilyForCharacters(
    const WebUChar* characters,
    size_t num_characters,
    const char* preferred_locale,
    WebKit::WebFontFamily* family) {
  NOTIMPLEMENTED();
}

void PpapiWebKitPlatformSupportImpl::SandboxSupport::getRenderStyleForStrike(
    const char* family, int sizeAndStyle, WebKit::WebFontRenderStyle* out) {
  NOTIMPLEMENTED();
}

#elif defined(OS_POSIX)

void
PpapiWebKitPlatformSupportImpl::SandboxSupport::getFontFamilyForCharacters(
    const WebUChar* characters,
    size_t num_characters,
    const char* preferred_locale,
    WebKit::WebFontFamily* family) {
  base::AutoLock lock(unicode_font_families_mutex_);
  const string16 key(characters, num_characters);
  const std::map<string16, WebKit::WebFontFamily>::const_iterator iter =
      unicode_font_families_.find(key);
  if (iter != unicode_font_families_.end()) {
    family->name = iter->second.name;
    family->isBold = iter->second.isBold;
    family->isItalic = iter->second.isItalic;
    return;
  }

  GetFontFamilyForCharacters(
      characters,
      num_characters,
      preferred_locale,
      family);
  unicode_font_families_.insert(make_pair(key, *family));
}

void PpapiWebKitPlatformSupportImpl::SandboxSupport::getRenderStyleForStrike(
    const char* family, int sizeAndStyle, WebKit::WebFontRenderStyle* out) {
  GetRenderStyleForStrike(family, sizeAndStyle, out);
}

#endif

PpapiWebKitPlatformSupportImpl::PpapiWebKitPlatformSupportImpl()
    : sandbox_support_(new PpapiWebKitPlatformSupportImpl::SandboxSupport()) {
}

PpapiWebKitPlatformSupportImpl::~PpapiWebKitPlatformSupportImpl() {
}

WebKit::WebClipboard* PpapiWebKitPlatformSupportImpl::clipboard() {
  NOTREACHED();
  return NULL;
}

WebKit::WebMimeRegistry* PpapiWebKitPlatformSupportImpl::mimeRegistry() {
  NOTREACHED();
  return NULL;
}

WebKit::WebFileUtilities* PpapiWebKitPlatformSupportImpl::fileUtilities() {
  NOTREACHED();
  return NULL;
}

WebKit::WebSandboxSupport* PpapiWebKitPlatformSupportImpl::sandboxSupport() {
  return sandbox_support_.get();
}

bool PpapiWebKitPlatformSupportImpl::sandboxEnabled() {
  return true;  // Assume PPAPI is always sandboxed.
}

unsigned long long PpapiWebKitPlatformSupportImpl::visitedLinkHash(
    const char* canonical_url,
    size_t length) {
  NOTREACHED();
  return 0;
}

bool PpapiWebKitPlatformSupportImpl::isLinkVisited(
    unsigned long long link_hash) {
  NOTREACHED();
  return false;
}

WebKit::WebMessagePortChannel*
PpapiWebKitPlatformSupportImpl::createMessagePortChannel() {
  NOTREACHED();
  return NULL;
}

void PpapiWebKitPlatformSupportImpl::setCookies(
    const WebKit::WebURL& url,
    const WebKit::WebURL& first_party_for_cookies,
    const WebKit::WebString& value) {
  NOTREACHED();
}

WebKit::WebString PpapiWebKitPlatformSupportImpl::cookies(
    const WebKit::WebURL& url,
    const WebKit::WebURL& first_party_for_cookies) {
  NOTREACHED();
  return WebKit::WebString();
}

void PpapiWebKitPlatformSupportImpl::prefetchHostName(
    const WebKit::WebString&) {
  NOTREACHED();
}

WebKit::WebString PpapiWebKitPlatformSupportImpl::defaultLocale() {
  NOTREACHED();
  return WebKit::WebString();
}

WebKit::WebThemeEngine* PpapiWebKitPlatformSupportImpl::themeEngine() {
  NOTREACHED();
  return NULL;
}

WebKit::WebURLLoader* PpapiWebKitPlatformSupportImpl::createURLLoader() {
  NOTREACHED();
  return NULL;
}

WebKit::WebSocketStreamHandle*
    PpapiWebKitPlatformSupportImpl::createSocketStreamHandle() {
  NOTREACHED();
  return NULL;
}

void PpapiWebKitPlatformSupportImpl::getPluginList(bool refresh,
    WebKit::WebPluginListBuilder* builder) {
  NOTREACHED();
}

WebKit::WebData PpapiWebKitPlatformSupportImpl::loadResource(const char* name) {
  NOTREACHED();
  return WebKit::WebData();
}

WebKit::WebStorageNamespace*
PpapiWebKitPlatformSupportImpl::createLocalStorageNamespace(
    const WebKit::WebString& path, unsigned quota) {
  NOTREACHED();
  return 0;
}

void PpapiWebKitPlatformSupportImpl::dispatchStorageEvent(
    const WebKit::WebString& key, const WebKit::WebString& old_value,
    const WebKit::WebString& new_value, const WebKit::WebString& origin,
    const WebKit::WebURL& url, bool is_local_storage) {
  NOTREACHED();
}

int PpapiWebKitPlatformSupportImpl::databaseDeleteFile(
    const WebKit::WebString& vfs_file_name, bool sync_dir) {
  NOTREACHED();
  return 0;
}

}  // namespace content
