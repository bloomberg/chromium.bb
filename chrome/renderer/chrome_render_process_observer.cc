// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_render_process_observer.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/content_settings_observer.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_thread.h"
#include "content/renderer/render_view.h"
#include "content/renderer/render_view_visitor.h"
#include "crypto/nss_util.h"
#include "third_party/sqlite/sqlite3.h"
#include "third_party/tcmalloc/chromium/src/google/malloc_extension.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCache.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCrossOriginPreflightResultCache.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFontCache.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "v8/include/v8.h"

#if defined(OS_WIN)
#include "app/win/iat_patch_function.h"
#endif

using WebKit::WebCache;
using WebKit::WebCrossOriginPreflightResultCache;
using WebKit::WebFontCache;

namespace {

#if defined(OS_WIN)

static app::win::IATPatchFunction g_iat_patch_createdca;
HDC WINAPI CreateDCAPatch(LPCSTR driver_name,
                          LPCSTR device_name,
                          LPCSTR output,
                          const void* init_data) {
  DCHECK(std::string("DISPLAY") == std::string(driver_name));
  DCHECK(!device_name);
  DCHECK(!output);
  DCHECK(!init_data);

  // CreateDC fails behind the sandbox, but not CreateCompatibleDC.
  return CreateCompatibleDC(NULL);
}

static app::win::IATPatchFunction g_iat_patch_get_font_data;
DWORD WINAPI GetFontDataPatch(HDC hdc,
                              DWORD table,
                              DWORD offset,
                              LPVOID buffer,
                              DWORD length) {
  int rv = GetFontData(hdc, table, offset, buffer, length);
  if (rv == GDI_ERROR && hdc) {
    HFONT font = static_cast<HFONT>(GetCurrentObject(hdc, OBJ_FONT));

    LOGFONT logfont;
    if (GetObject(font, sizeof(LOGFONT), &logfont)) {
      std::vector<char> font_data;
      if (RenderThread::current()->Send(new ViewHostMsg_PreCacheFont(logfont)))
        rv = GetFontData(hdc, table, offset, buffer, length);
    }
  }
  return rv;
}

#endif

class RenderViewContentSettingsSetter : public RenderViewVisitor {
 public:
  RenderViewContentSettingsSetter(const GURL& url,
                                  const ContentSettings& content_settings)
      : url_(url),
        content_settings_(content_settings) {
  }

  virtual bool Visit(RenderView* render_view) {
    if (GURL(render_view->webview()->mainFrame()->url()) == url_) {
      ContentSettingsObserver::Get(render_view)->SetContentSettings(
          content_settings_);
    }
    return true;
  }

 private:
  GURL url_;
  ContentSettings content_settings_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewContentSettingsSetter);
};

}  // namespace

bool ChromeRenderProcessObserver::is_incognito_process_ = false;

ChromeRenderProcessObserver::ChromeRenderProcessObserver() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kEnableWatchdog)) {
    // TODO(JAR): Need to implement renderer IO msgloop watchdog.
  }

  if (command_line.HasSwitch(switches::kDumpHistogramsOnExit)) {
    base::StatisticsRecorder::set_dump_on_exit(true);
  }

#if defined(OS_WIN)
  // Need to patch a few functions for font loading to work correctly.
  FilePath pdf;
  if (PathService::Get(chrome::FILE_PDF_PLUGIN, &pdf) &&
      file_util::PathExists(pdf)) {
    g_iat_patch_createdca.Patch(
        pdf.value().c_str(), "gdi32.dll", "CreateDCA", CreateDCAPatch);
    g_iat_patch_get_font_data.Patch(
        pdf.value().c_str(), "gdi32.dll", "GetFontData", GetFontDataPatch);
  }
#endif

#if defined(OS_LINUX)
  // Remoting requires NSS to function properly.
  if (!command_line.HasSwitch(switches::kSingleProcess) &&
      command_line.HasSwitch(switches::kEnableRemoting)) {
#if defined(USE_NSS)
    // We are going to fork to engage the sandbox and we have not loaded
    // any security modules so it is safe to disable the fork check in NSS.
    crypto::DisableNSSForkCheck();
    crypto::ForceNSSNoDBInit();
    crypto::EnsureNSSInit();
#else
    // TODO(bulach): implement openssl support.
    NOTREACHED() << "Remoting is not supported for openssl";
#endif
  }
#endif
}

ChromeRenderProcessObserver::~ChromeRenderProcessObserver() {
}

bool ChromeRenderProcessObserver::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeRenderProcessObserver, message)
    IPC_MESSAGE_HANDLER(ViewMsg_SetIsIncognitoProcess, OnSetIsIncognitoProcess)
    IPC_MESSAGE_HANDLER(ViewMsg_SetContentSettingsForCurrentURL,
                        OnSetContentSettingsForCurrentURL)
    IPC_MESSAGE_HANDLER(ViewMsg_SetCacheCapacities, OnSetCacheCapacities)
    IPC_MESSAGE_HANDLER(ViewMsg_ClearCache, OnClearCache)
#if defined(USE_TCMALLOC)
    IPC_MESSAGE_HANDLER(ViewMsg_GetRendererTcmalloc, OnGetRendererTcmalloc)
#endif
    IPC_MESSAGE_HANDLER(ViewMsg_GetV8HeapStats, OnGetV8HeapStats)
    IPC_MESSAGE_HANDLER(ViewMsg_GetCacheResourceStats, OnGetCacheResourceStats)
    IPC_MESSAGE_HANDLER(ViewMsg_PurgeMemory, OnPurgeMemory)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ChromeRenderProcessObserver::OnSetIsIncognitoProcess(
    bool is_incognito_process) {
  is_incognito_process_ = is_incognito_process;
}

void ChromeRenderProcessObserver::OnSetContentSettingsForCurrentURL(
    const GURL& url,
    const ContentSettings& content_settings) {
  RenderViewContentSettingsSetter setter(url, content_settings);
  RenderView::ForEach(&setter);
}

void ChromeRenderProcessObserver::OnSetCacheCapacities(size_t min_dead_capacity,
                                                       size_t max_dead_capacity,
                                                       size_t capacity) {
  WebCache::setCapacities(
      min_dead_capacity, max_dead_capacity, capacity);
}

void ChromeRenderProcessObserver::OnClearCache() {
  WebCache::clear();
}

void ChromeRenderProcessObserver::OnGetCacheResourceStats() {
  WebCache::ResourceTypeStats stats;
  WebCache::getResourceTypeStats(&stats);
  Send(new ViewHostMsg_ResourceTypeStats(stats));
}

#if defined(USE_TCMALLOC)
void ChromeRenderProcessObserver::OnGetRendererTcmalloc() {
  std::string result;
  char buffer[1024 * 32];
  base::ProcessId pid = base::GetCurrentProcId();
  MallocExtension::instance()->GetStats(buffer, sizeof(buffer));
  result.append(buffer);
  Send(new ViewHostMsg_RendererTcmalloc(pid, result));
}
#endif

void ChromeRenderProcessObserver::OnGetV8HeapStats() {
  v8::HeapStatistics heap_stats;
  v8::V8::GetHeapStatistics(&heap_stats);
  Send(new ViewHostMsg_V8HeapStats(heap_stats.total_heap_size(),
                                   heap_stats.used_heap_size()));
}

void ChromeRenderProcessObserver::OnPurgeMemory() {
  // Clear the object cache (as much as possible; some live objects cannot be
  // freed).
  WebCache::clear();

  // Clear the font/glyph cache.
  WebFontCache::clear();

  // Clear the Cross-Origin Preflight cache.
  WebCrossOriginPreflightResultCache::clear();

  // Release all freeable memory from the SQLite process-global page cache (a
  // low-level object which backs the Connection-specific page caches).
  while (sqlite3_release_memory(std::numeric_limits<int>::max()) > 0) {
  }

  // Repeatedly call the V8 idle notification until it returns true ("nothing
  // more to free").  Note that it makes more sense to do this than to implement
  // a new "delete everything" pass because object references make it difficult
  // to free everything possible in just one pass.
  while (!v8::V8::IdleNotification()) {
  }

#if (defined(OS_WIN) || defined(OS_LINUX)) && defined(USE_TCMALLOC)
  // Tell tcmalloc to release any free pages it's still holding.
  MallocExtension::instance()->ReleaseFreeMemory();
#endif
}
