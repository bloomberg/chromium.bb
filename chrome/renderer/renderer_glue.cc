// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides the embedder's side of random webkit glue functions.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <vector>

#include "base/command_line.h"
#include "base/ref_counted.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/socket_stream_dispatcher.h"
#include "chrome/common/url_constants.h"
#include "chrome/plugin/npobject_util.h"
#include "chrome/renderer/net/renderer_net_predictor.h"
#include "chrome/renderer/render_process.h"
#include "chrome/renderer/render_thread.h"
#include "googleurl/src/url_util.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKitClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/glue/scoped_clipboard_writer_glue.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/websocketstreamhandle_bridge.h"

#if !defined(DISABLE_NACL)
#include "native_client/src/shared/imc/nacl_imc.h"
#include "native_client/src/trusted/plugin/nacl_entry_points.h"
#endif

#if defined(OS_WIN)
#include <strsafe.h>  // note: per msdn docs, this must *follow* other includes
#elif defined(OS_LINUX)
#include "chrome/renderer/renderer_sandbox_support_linux.h"
#endif

// This definition of WriteBitmapFromPixels uses shared memory to communicate
// across processes.
void ScopedClipboardWriterGlue::WriteBitmapFromPixels(const void* pixels,
                                                      const gfx::Size& size) {
  // Do not try to write a bitmap more than once
  if (shared_buf_)
    return;

  uint32 buf_size = 4 * size.width() * size.height();

  // Allocate a shared memory buffer to hold the bitmap bits.
#if defined(OS_POSIX)
  // On POSIX, we need to ask the browser to create the shared memory for us,
  // since this is blocked by the sandbox.
  base::SharedMemoryHandle shared_mem_handle;
  ViewHostMsg_AllocateSharedMemoryBuffer *msg =
      new ViewHostMsg_AllocateSharedMemoryBuffer(buf_size,
                                                 &shared_mem_handle);
  if (RenderThread::current()->Send(msg)) {
    if (base::SharedMemory::IsHandleValid(shared_mem_handle)) {
      shared_buf_ = new base::SharedMemory(shared_mem_handle, false);
      if (!shared_buf_ || !shared_buf_->Map(buf_size)) {
        NOTREACHED() << "Map failed";
        return;
      }
    } else {
      NOTREACHED() << "Browser failed to allocate shared memory";
      return;
    }
  } else {
    NOTREACHED() << "Browser allocation request message failed";
    return;
  }
#else  // !OS_POSIX
  shared_buf_ = new base::SharedMemory;
  if (!shared_buf_->CreateAndMapAnonymous(buf_size)) {
    NOTREACHED();
    return;
  }
#endif

  // Copy the bits into shared memory
  memcpy(shared_buf_->memory(), pixels, buf_size);
  shared_buf_->Unmap();

  ui::Clipboard::ObjectMapParam size_param;
  const char* size_data = reinterpret_cast<const char*>(&size);
  for (size_t i = 0; i < sizeof(gfx::Size); ++i)
    size_param.push_back(size_data[i]);

  ui::Clipboard::ObjectMapParams params;

  // The first parameter is replaced on the receiving end with a pointer to
  // a shared memory object containing the bitmap. We reserve space for it here.
  ui::Clipboard::ObjectMapParam place_holder_param;
  params.push_back(place_holder_param);
  params.push_back(size_param);
  objects_[ui::Clipboard::CBF_SMBITMAP] = params;
}

// Define a destructor that makes IPCs to flush the contents to the
// system clipboard.
ScopedClipboardWriterGlue::~ScopedClipboardWriterGlue() {
  if (objects_.empty())
    return;

  if (shared_buf_) {
    RenderThread::current()->Send(
        new ViewHostMsg_ClipboardWriteObjectsSync(objects_,
                shared_buf_->handle()));
    delete shared_buf_;
    return;
  }

  RenderThread::current()->Send(
      new ViewHostMsg_ClipboardWriteObjectsAsync(objects_));
}

namespace webkit_glue {

void PrecacheUrl(const wchar_t* url, int url_length) {
  // TBD: jar: Need implementation that loads the targetted URL into our cache.
  // For now, at least prefetch DNS lookup
  std::string url_string;
  WideToUTF8(url, url_length, &url_string);
  const std::string host = GURL(url_string).host();
  if (!host.empty())
    DnsPrefetchCString(host.data(), host.length());
}

void AppendToLog(const char* file, int line, const char* msg) {
  logging::LogMessage(file, line).stream() << msg;
}

base::StringPiece GetDataResource(int resource_id) {
  return ResourceBundle::GetSharedInstance().GetRawDataResource(resource_id);
}

#if defined(OS_WIN)
HCURSOR LoadCursor(int cursor_id) {
  return ResourceBundle::GetSharedInstance().LoadCursor(cursor_id);
}
#endif

// Clipboard glue

ui::Clipboard* ClipboardGetClipboard() {
  return NULL;
}

bool ClipboardIsFormatAvailable(const ui::Clipboard::FormatType& format,
                                ui::Clipboard::Buffer buffer) {
  bool result;
  RenderThread::current()->Send(
      new ViewHostMsg_ClipboardIsFormatAvailable(format, buffer, &result));
  return result;
}

void ClipboardReadText(ui::Clipboard::Buffer buffer, string16* result) {
  RenderThread::current()->Send(new ViewHostMsg_ClipboardReadText(buffer,
                                                                  result));
}

void ClipboardReadAsciiText(ui::Clipboard::Buffer buffer, std::string* result) {
  RenderThread::current()->Send(new ViewHostMsg_ClipboardReadAsciiText(buffer,
                                                                       result));
}

void ClipboardReadHTML(ui::Clipboard::Buffer buffer, string16* markup,
                       GURL* url) {
  RenderThread::current()->Send(new ViewHostMsg_ClipboardReadHTML(buffer,
                                                                  markup, url));
}

bool ClipboardReadAvailableTypes(ui::Clipboard::Buffer buffer,
                                 std::vector<string16>* types,
                                 bool* contains_filenames) {
  bool result = false;
  RenderThread::current()->Send(new ViewHostMsg_ClipboardReadAvailableTypes(
      buffer, &result, types, contains_filenames));
  return result;
}

bool ClipboardReadData(ui::Clipboard::Buffer buffer, const string16& type,
                       string16* data, string16* metadata) {
  bool result = false;
  RenderThread::current()->Send(new ViewHostMsg_ClipboardReadData(
      buffer, type, &result, data, metadata));
  return result;
}

bool ClipboardReadFilenames(ui::Clipboard::Buffer buffer,
                            std::vector<string16>* filenames) {
  bool result;
  RenderThread::current()->Send(new ViewHostMsg_ClipboardReadFilenames(
      buffer, &result, filenames));
  return result;
}

void GetPlugins(bool refresh,
                std::vector<webkit::npapi::WebPluginInfo>* plugins) {
  if (!RenderThread::current()->plugin_refresh_allowed())
    refresh = false;
  RenderThread::current()->Send(new ViewHostMsg_GetPlugins(refresh, plugins));
}

bool IsProtocolSupportedForMedia(const GURL& url) {
  // If new protocol is to be added here, we need to make sure the response is
  // validated accordingly in the media engine.
  if (url.SchemeIsFile() || url.SchemeIs(chrome::kHttpScheme) ||
      url.SchemeIs(chrome::kHttpsScheme) ||
      url.SchemeIs(chrome::kDataScheme) ||
      url.SchemeIs(chrome::kExtensionScheme) ||
      url.SchemeIs(chrome::kBlobScheme))
    return true;
  return false;
}

// static factory function
ResourceLoaderBridge* ResourceLoaderBridge::Create(
    const ResourceLoaderBridge::RequestInfo& request_info) {
  return ChildThread::current()->CreateBridge(request_info, -1, -1);
}

// static factory function
WebSocketStreamHandleBridge* WebSocketStreamHandleBridge::Create(
    WebKit::WebSocketStreamHandle* handle,
    WebSocketStreamHandleDelegate* delegate) {
  SocketStreamDispatcher* dispatcher =
      ChildThread::current()->socket_stream_dispatcher();
  return dispatcher->CreateBridge(handle, delegate);
}

void CloseCurrentConnections() {
  RenderThread::current()->CloseCurrentConnections();
}

void SetCacheMode(bool enabled) {
  RenderThread::current()->SetCacheMode(enabled);
}

void ClearCache(bool preserve_ssl_host_info) {
  RenderThread::current()->ClearCache(preserve_ssl_host_info);
}

std::string GetProductVersion() {
  chrome::VersionInfo version_info;
  std::string product("Chrome/");
  product += version_info.is_valid() ? version_info.Version()
                                     : "0.0.0.0";
  return product;
}

bool IsSingleProcess() {
  return CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess);
}

void EnableSpdy(bool enable) {
  RenderThread::current()->EnableSpdy(enable);
}

void UserMetricsRecordAction(const std::string& action) {
  RenderThread::current()->Send(
      new ViewHostMsg_UserMetricsRecordAction(action));
}

#if !defined(DISABLE_NACL)
bool LaunchSelLdr(const char* alleged_url, int socket_count, void* imc_handles,
                  void* nacl_process_handle, int* nacl_process_id) {
  std::vector<nacl::FileDescriptor> sockets;
  base::ProcessHandle nacl_process;
  if (!RenderThread::current()->Send(
    new ViewHostMsg_LaunchNaCl(
        ASCIIToWide(alleged_url),
        socket_count,
        &sockets,
        &nacl_process,
        reinterpret_cast<base::ProcessId*>(nacl_process_id)))) {
    return false;
  }
  CHECK(static_cast<int>(sockets.size()) == socket_count);
  for (int i = 0; i < socket_count; i++) {
    static_cast<nacl::Handle*>(imc_handles)[i] =
        nacl::ToNativeHandle(sockets[i]);
  }
  *static_cast<nacl::Handle*>(nacl_process_handle) = nacl_process;
  return true;
}
#endif

#if defined(OS_LINUX)
int MatchFontWithFallback(const std::string& face, bool bold,
                          bool italic, int charset) {
  return renderer_sandbox_support::MatchFontWithFallback(
      face, bold, italic, charset);
}

bool GetFontTable(int fd, uint32_t table, uint8_t* output,
                  size_t* output_length) {
  return renderer_sandbox_support::GetFontTable(
      fd, table, output, output_length);
}
#endif

}  // namespace webkit_glue
