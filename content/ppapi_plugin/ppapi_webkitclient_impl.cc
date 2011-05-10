// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/ppapi_plugin/ppapi_webkitclient_impl.h"

#include <map>

#include "base/synchronization/lock.h"
#include "base/logging.h"
#include "base/string16.h"
#include "build/build_config.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSerializedScriptValue.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"

#if defined(OS_WIN)
#include "third_party/WebKit/Source/WebKit/chromium/public/win/WebSandboxSupport.h"
#elif defined(OS_MACOSX)
#include "third_party/WebKit/Source/WebKit/chromium/public/mac/WebSandboxSupport.h"
#elif defined(OS_LINUX)
#include "content/common/child_process_sandbox_support_linux.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/linux/WebSandboxSupport.h"
#endif

using WebKit::WebSandboxSupport;
using WebKit::WebString;
using WebKit::WebUChar;

class PpapiWebKitClientImpl::SandboxSupport : public WebSandboxSupport {
 public:
#if defined(OS_WIN)
  virtual bool ensureFontLoaded(HFONT);
#elif defined(OS_MACOSX)
  virtual bool loadFont(NSFont* srcFont, ATSFontContainerRef* out);
#elif defined(OS_LINUX)
  virtual WebString getFontFamilyForCharacters(
      const WebUChar* characters,
      size_t numCharacters,
      const char* preferred_locale);
  virtual void getRenderStyleForStrike(
      const char* family, int sizeAndStyle, WebKit::WebFontRenderStyle* out);

 private:
  // WebKit likes to ask us for the correct font family to use for a set of
  // unicode code points. It needs this information frequently so we cache it
  // here. The key in this map is an array of 16-bit UTF16 values from WebKit.
  // The value is a string containing the correct font family.
  base::Lock unicode_font_families_mutex_;
  std::map<string16, std::string> unicode_font_families_;
#endif
};

#if defined(OS_WIN)

bool PpapiWebKitClientImpl::SandboxSupport::ensureFontLoaded(HFONT font) {
  // TODO(brettw) this should do the something similar to what
  // RendererWebKitClientImpl does and request that the browser load the font.
  NOTIMPLEMENTED();
  return false;
}

#elif defined(OS_MACOSX)

bool PpapiWebKitClientImpl::SandboxSupport::loadFont(NSFont* srcFont,
    ATSFontContainerRef* out) {
  // TODO(brettw) this should do the something similar to what
  // RendererWebKitClientImpl does and request that the browser load the font.
  NOTIMPLEMENTED();
  return false;
}

#elif defined(OS_LINUX)

WebString PpapiWebKitClientImpl::SandboxSupport::getFontFamilyForCharacters(
    const WebUChar* characters,
    size_t num_characters,
    const char* preferred_locale) {
  base::AutoLock lock(unicode_font_families_mutex_);
  const string16 key(characters, num_characters);
  const std::map<string16, std::string>::const_iterator iter =
      unicode_font_families_.find(key);
  if (iter != unicode_font_families_.end())
    return WebString::fromUTF8(iter->second);

  const std::string family_name =
      child_process_sandbox_support::getFontFamilyForCharacters(
          characters,
          num_characters,
          preferred_locale);
  unicode_font_families_.insert(make_pair(key, family_name));
  return WebString::fromUTF8(family_name);
}

void PpapiWebKitClientImpl::SandboxSupport::getRenderStyleForStrike(
    const char* family, int sizeAndStyle, WebKit::WebFontRenderStyle* out) {
  child_process_sandbox_support::getRenderStyleForStrike(family, sizeAndStyle,
                                                         out);
}

#endif

PpapiWebKitClientImpl::PpapiWebKitClientImpl() {
}

PpapiWebKitClientImpl::~PpapiWebKitClientImpl() {
}

WebKit::WebClipboard* PpapiWebKitClientImpl::clipboard() {
  NOTREACHED();
  return NULL;
}

WebKit::WebMimeRegistry* PpapiWebKitClientImpl::mimeRegistry() {
  NOTREACHED();
  return NULL;
}

WebKit::WebFileUtilities* PpapiWebKitClientImpl::fileUtilities() {
  NOTREACHED();
  return NULL;
}

WebKit::WebSandboxSupport* PpapiWebKitClientImpl::sandboxSupport() {
  return sandbox_support_.get();
}

bool PpapiWebKitClientImpl::sandboxEnabled() {
  return true;  // Assume PPAPI is always sandboxed.
}

unsigned long long PpapiWebKitClientImpl::visitedLinkHash(
    const char* canonical_url,
    size_t length) {
  NOTREACHED();
  return 0;
}

bool PpapiWebKitClientImpl::isLinkVisited(unsigned long long link_hash) {
  NOTREACHED();
  return false;
}

WebKit::WebMessagePortChannel*
PpapiWebKitClientImpl::createMessagePortChannel() {
  NOTREACHED();
  return NULL;
}

void PpapiWebKitClientImpl::setCookies(
    const WebKit::WebURL& url,
    const WebKit::WebURL& first_party_for_cookies,
    const WebKit::WebString& value) {
  NOTREACHED();
}

WebKit::WebString PpapiWebKitClientImpl::cookies(
    const WebKit::WebURL& url,
    const WebKit::WebURL& first_party_for_cookies) {
  NOTREACHED();
  return WebKit::WebString();
}

void PpapiWebKitClientImpl::prefetchHostName(const WebKit::WebString&) {
  NOTREACHED();
}

WebKit::WebString PpapiWebKitClientImpl::defaultLocale() {
  NOTREACHED();
  return WebKit::WebString();
}

WebKit::WebThemeEngine* PpapiWebKitClientImpl::themeEngine() {
  NOTREACHED();
  return NULL;
}

WebKit::WebURLLoader* PpapiWebKitClientImpl::createURLLoader() {
  NOTREACHED();
  return NULL;
}

WebKit::WebSocketStreamHandle*
    PpapiWebKitClientImpl::createSocketStreamHandle() {
  NOTREACHED();
  return NULL;
}

void PpapiWebKitClientImpl::getPluginList(bool refresh,
    WebKit::WebPluginListBuilder* builder) {
  NOTREACHED();
}

WebKit::WebData PpapiWebKitClientImpl::loadResource(const char* name) {
  NOTREACHED();
  return WebKit::WebData();
}

WebKit::WebStorageNamespace*
PpapiWebKitClientImpl::createLocalStorageNamespace(
    const WebKit::WebString& path, unsigned quota) {
  NOTREACHED();
  return 0;
}

void PpapiWebKitClientImpl::dispatchStorageEvent(
    const WebKit::WebString& key, const WebKit::WebString& old_value,
    const WebKit::WebString& new_value, const WebKit::WebString& origin,
    const WebKit::WebURL& url, bool is_local_storage) {
  NOTREACHED();
}

WebKit::WebSharedWorkerRepository*
PpapiWebKitClientImpl::sharedWorkerRepository() {
  NOTREACHED();
  return NULL;
}

int PpapiWebKitClientImpl::databaseDeleteFile(
    const WebKit::WebString& vfs_file_name, bool sync_dir) {
  NOTREACHED();
  return 0;
}

void PpapiWebKitClientImpl::createIDBKeysFromSerializedValuesAndKeyPath(
    const WebKit::WebVector<WebKit::WebSerializedScriptValue>& values,
    const WebKit::WebString& keyPath,
    WebKit::WebVector<WebKit::WebIDBKey>& keys) {
  NOTREACHED();
}

WebKit::WebSerializedScriptValue
PpapiWebKitClientImpl::injectIDBKeyIntoSerializedValue(
    const WebKit::WebIDBKey& key,
    const WebKit::WebSerializedScriptValue& value,
    const WebKit::WebString& keyPath) {
  NOTREACHED();
  return WebKit::WebSerializedScriptValue();
}
