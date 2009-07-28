// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/renderer/renderer_webkitclient_impl.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/platform_file.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/db_message_filter.h"
#include "chrome/common/render_messages.h"
#include "chrome/plugin/npobject_util.h"
#include "chrome/renderer/net/render_dns_master.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/renderer_webstoragenamespace_impl.h"
#include "chrome/renderer/visitedlink_slave.h"
#include "webkit/api/public/WebString.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webkit_glue.h"

#if defined(OS_LINUX)
#include "chrome/renderer/renderer_sandbox_support_linux.h"
#endif

using WebKit::WebStorageArea;
using WebKit::WebStorageNamespace;
using WebKit::WebString;
using WebKit::WebURL;

//------------------------------------------------------------------------------

WebKit::WebClipboard* RendererWebKitClientImpl::clipboard() {
  return &clipboard_;
}

WebKit::WebMimeRegistry* RendererWebKitClientImpl::mimeRegistry() {
  return &mime_registry_;
}

WebKit::WebSandboxSupport* RendererWebKitClientImpl::sandboxSupport() {
#if defined(OS_WIN) || defined(OS_LINUX)
  return &sandbox_support_;
#else
  return NULL;
#endif
}

bool RendererWebKitClientImpl::getFileSize(const WebString& path,
                                           long long& result) {
  if (RenderThread::current()->Send(new ViewHostMsg_GetFileSize(
      FilePath(webkit_glue::WebStringToFilePathString(path)),
      reinterpret_cast<int64*>(&result)))) {
    return result >= 0;
  } else {
    result = -1;
    return false;
  }
}

unsigned long long RendererWebKitClientImpl::visitedLinkHash(
    const char* canonical_url,
    size_t length) {
  return RenderThread::current()->visited_link_slave()->ComputeURLFingerprint(
      canonical_url, length);
}

bool RendererWebKitClientImpl::isLinkVisited(unsigned long long link_hash) {
  return RenderThread::current()->visited_link_slave()->IsVisited(link_hash);
}

void RendererWebKitClientImpl::setCookies(const WebURL& url,
                                          const WebURL& first_party_for_cookies,
                                          const WebString& value) {
  std::string value_utf8;
  UTF16ToUTF8(value.data(), value.length(), &value_utf8);
  RenderThread::current()->Send(
      new ViewHostMsg_SetCookie(url, first_party_for_cookies, value_utf8));
}

WebString RendererWebKitClientImpl::cookies(
    const WebURL& url, const WebURL& first_party_for_cookies) {
  std::string value_utf8;
  RenderThread::current()->Send(
      new ViewHostMsg_GetCookies(url, first_party_for_cookies, &value_utf8));
  return WebString::fromUTF8(value_utf8);
}

void RendererWebKitClientImpl::prefetchHostName(const WebString& hostname) {
  if (!hostname.isEmpty()) {
    std::string hostname_utf8;
    UTF16ToUTF8(hostname.data(), hostname.length(), &hostname_utf8);
    DnsPrefetchCString(hostname_utf8.data(), hostname_utf8.length());
  }
}

WebString RendererWebKitClientImpl::defaultLocale() {
  // TODO(darin): Eliminate this webkit_glue call.
  return WideToUTF16(webkit_glue::GetWebKitLocale());
}

void RendererWebKitClientImpl::suddenTerminationChanged(bool enabled) {
  RenderThread* thread = RenderThread::current();
  if (thread)  // NULL in unittests.
    thread->Send(new ViewHostMsg_SuddenTerminationChanged(enabled));
}

WebStorageNamespace* RendererWebKitClientImpl::createLocalStorageNamespace(
    const WebString& path) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return WebStorageNamespace::createLocalStorageNamespace(path);
  // The browser process decides the path, so ignore that param.
  return new RendererWebStorageNamespaceImpl(true);
}

WebStorageNamespace* RendererWebKitClientImpl::createSessionStorageNamespace() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return WebStorageNamespace::createSessionStorageNamespace();
  return new RendererWebStorageNamespaceImpl(false);
}

//------------------------------------------------------------------------------

WebString RendererWebKitClientImpl::MimeRegistry::mimeTypeForExtension(
    const WebString& file_extension) {
  if (IsPluginProcess())
    return SimpleWebMimeRegistryImpl::mimeTypeForExtension(file_extension);

  // The sandbox restricts our access to the registry, so we need to proxy
  // these calls over to the browser process.
  std::string mime_type;
  RenderThread::current()->Send(new ViewHostMsg_GetMimeTypeFromExtension(
      webkit_glue::WebStringToFilePathString(file_extension), &mime_type));
  return ASCIIToUTF16(mime_type);

}

WebString RendererWebKitClientImpl::MimeRegistry::mimeTypeFromFile(
    const WebString& file_path) {
  if (IsPluginProcess())
    return SimpleWebMimeRegistryImpl::mimeTypeFromFile(file_path);

  // The sandbox restricts our access to the registry, so we need to proxy
  // these calls over to the browser process.
  std::string mime_type;
  RenderThread::current()->Send(new ViewHostMsg_GetMimeTypeFromFile(
      FilePath(webkit_glue::WebStringToFilePathString(file_path)),
      &mime_type));
  return ASCIIToUTF16(mime_type);

}

WebString RendererWebKitClientImpl::MimeRegistry::preferredExtensionForMIMEType(
    const WebString& mime_type) {
  if (IsPluginProcess())
    return SimpleWebMimeRegistryImpl::preferredExtensionForMIMEType(mime_type);

  // The sandbox restricts our access to the registry, so we need to proxy
  // these calls over to the browser process.
  FilePath::StringType file_extension;
  RenderThread::current()->Send(
      new ViewHostMsg_GetPreferredExtensionForMimeType(UTF16ToASCII(mime_type),
          &file_extension));
  return webkit_glue::FilePathStringToWebString(file_extension);
}

//------------------------------------------------------------------------------

#if defined(OS_WIN)

bool RendererWebKitClientImpl::SandboxSupport::ensureFontLoaded(HFONT font) {
  LOGFONT logfont;
  GetObject(font, sizeof(LOGFONT), &logfont);
  return RenderThread::current()->Send(new ViewHostMsg_LoadFont(logfont));
}

#elif defined(OS_LINUX)

WebString RendererWebKitClientImpl::SandboxSupport::getFontFamilyForCharacters(
    const WebKit::WebUChar* characters, size_t num_characters) {
  AutoLock lock(unicode_font_families_mutex_);
  const std::string key(reinterpret_cast<const char*>(characters),
                        num_characters * sizeof(characters[0]));
  const std::map<std::string, std::string>::const_iterator iter =
      unicode_font_families_.find(key);
  if (iter != unicode_font_families_.end())
    return WebString::fromUTF8(iter->second.data(), iter->second.size());

  const std::string family_name =
      renderer_sandbox_support::getFontFamilyForCharacters(characters,
                                                           num_characters);
  unicode_font_families_.insert(make_pair(key, family_name));
  return WebString::fromUTF8(family_name);
}

#endif

//------------------------------------------------------------------------------

base::PlatformFile RendererWebKitClientImpl::databaseOpenFile(
  const WebString& file_name, int desired_flags) {
  DBMessageFilter* db_message_filter = DBMessageFilter::GetInstance();
  int message_id = db_message_filter->GetUniqueID();
  return db_message_filter->SendAndWait(
    new ViewHostMsg_DatabaseOpenFile(
      FilePath(webkit_glue::WebStringToFilePathString(file_name)),
      desired_flags, message_id),
    message_id, base::kInvalidPlatformFileValue);
}

bool RendererWebKitClientImpl::databaseDeleteFile(const WebString& file_name) {
  DBMessageFilter* db_message_filter = DBMessageFilter::GetInstance();
  int message_id = db_message_filter->GetUniqueID();
  return db_message_filter->SendAndWait(
    new ViewHostMsg_DatabaseDeleteFile(
      FilePath(webkit_glue::WebStringToFilePathString(file_name)),
      message_id),
    message_id, false);
}

long RendererWebKitClientImpl::databaseGetFileAttributes(
  const WebString& file_name) {
  DBMessageFilter* db_message_filter = DBMessageFilter::GetInstance();
  int message_id = db_message_filter->GetUniqueID();
  return db_message_filter->SendAndWait(
    new ViewHostMsg_DatabaseGetFileAttributes(
      FilePath(webkit_glue::WebStringToFilePathString(file_name)),
      message_id),
    message_id, -1L);
}

long long RendererWebKitClientImpl::databaseGetFileSize(
  const WebString& file_name) {
  DBMessageFilter* db_message_filter = DBMessageFilter::GetInstance();
  int message_id = db_message_filter->GetUniqueID();
  return db_message_filter->SendAndWait(
    new ViewHostMsg_DatabaseGetFileSize(
      FilePath(webkit_glue::WebStringToFilePathString(file_name)),
      message_id),
    message_id, 0LL);
}
