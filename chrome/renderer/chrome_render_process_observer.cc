// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_render_process_observer.h"

#include <limits>
#include <vector>

#include "base/allocator/allocator_extension.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/threading/platform_thread.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_localization_peer.h"
#include "chrome/common/metrics/variations/variations_util.h"
#include "chrome/common/net/net_resource_provider.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/renderer/content_settings_observer.h"
#include "chrome/renderer/security_filter_peer.h"
#include "content/public/common/resource_dispatcher_delegate.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view_visitor.h"
#include "content/public/renderer/render_view.h"
#include "crypto/nss_util.h"
#include "media/base/media_switches.h"
#include "net/base/net_errors.h"
#include "net/base/net_module.h"
#include "third_party/sqlite/sqlite3.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCache.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCrossOriginPreflightResultCache.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFontCache.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRuntimeFeatures.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "v8/include/v8.h"

#if defined(OS_WIN)
#include "base/win/iat_patch_function.h"
#endif

using WebKit::WebCache;
using WebKit::WebCrossOriginPreflightResultCache;
using WebKit::WebFontCache;
using WebKit::WebRuntimeFeatures;
using content::RenderThread;

namespace {

static const int kCacheStatsDelayMS = 2000;

class RendererResourceDelegate : public content::ResourceDispatcherDelegate {
 public:
  RendererResourceDelegate()
      : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  }

  virtual webkit_glue::ResourceLoaderBridge::Peer* OnRequestComplete(
      webkit_glue::ResourceLoaderBridge::Peer* current_peer,
      ResourceType::Type resource_type,
      int error_code) OVERRIDE {
    // Update the browser about our cache.
    // Rate limit informing the host of our cache stats.
    if (!weak_factory_.HasWeakPtrs()) {
      MessageLoop::current()->PostDelayedTask(
         FROM_HERE,
         base::Bind(&RendererResourceDelegate::InformHostOfCacheStats,
                    weak_factory_.GetWeakPtr()),
         base::TimeDelta::FromMilliseconds(kCacheStatsDelayMS));
    }

    if (error_code == net::ERR_ABORTED) {
      return NULL;
    }

    // Resource canceled with a specific error are filtered.
    return SecurityFilterPeer::CreateSecurityFilterPeerForDeniedRequest(
        resource_type, current_peer, error_code);
  }

  virtual webkit_glue::ResourceLoaderBridge::Peer* OnReceivedResponse(
      webkit_glue::ResourceLoaderBridge::Peer* current_peer,
      const std::string& mime_type,
      const GURL& url) OVERRIDE {
    return ExtensionLocalizationPeer::CreateExtensionLocalizationPeer(
        current_peer, RenderThread::Get(), mime_type, url);
  }

 private:
  void InformHostOfCacheStats() {
    WebCache::UsageStats stats;
    WebCache::getUsageStats(&stats);
    RenderThread::Get()->Send(new ChromeViewHostMsg_UpdatedCacheStats(stats));
  }

  base::WeakPtrFactory<RendererResourceDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RendererResourceDelegate);
};

#if defined(OS_WIN)
static base::win::IATPatchFunction g_iat_patch_createdca;
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

static base::win::IATPatchFunction g_iat_patch_get_font_data;
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
      RenderThread::Get()->PreCacheFont(logfont);
      rv = GetFontData(hdc, table, offset, buffer, length);
      RenderThread::Get()->ReleaseCachedFonts();
    }
  }
  return rv;
}
#endif  // OS_WIN

}  // namespace

bool ChromeRenderProcessObserver::is_incognito_process_ = false;

bool ChromeRenderProcessObserver::extension_activity_log_enabled_ = false;

ChromeRenderProcessObserver::ChromeRenderProcessObserver(
    chrome::ChromeContentRendererClient* client)
    : client_(client),
      clear_cache_pending_(false) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kEnableWatchdog)) {
    // TODO(JAR): Need to implement renderer IO msgloop watchdog.
  }

  if (command_line.HasSwitch(switches::kDumpHistogramsOnExit)) {
    base::StatisticsRecorder::set_dump_on_exit(true);
  }

#if defined(ENABLE_AUTOFILL_DIALOG)
  WebRuntimeFeatures::enableRequestAutocomplete(
      command_line.HasSwitch(switches::kEnableInteractiveAutocomplete) ||
      command_line.HasSwitch(switches::kEnableExperimentalWebKitFeatures));
#endif

  RenderThread* thread = RenderThread::Get();
  resource_delegate_.reset(new RendererResourceDelegate());
  thread->SetResourceDispatcherDelegate(resource_delegate_.get());

  // Configure modules that need access to resources.
  net::NetModule::SetResourceProvider(chrome_common_net::NetResourceProvider);

#if defined(OS_WIN)
  // Need to patch a few functions for font loading to work correctly.
  base::FilePath pdf;
  if (PathService::Get(chrome::FILE_PDF_PLUGIN, &pdf) &&
      file_util::PathExists(pdf)) {
    g_iat_patch_createdca.Patch(
        pdf.value().c_str(), "gdi32.dll", "CreateDCA", CreateDCAPatch);
    g_iat_patch_get_font_data.Patch(
        pdf.value().c_str(), "gdi32.dll", "GetFontData", GetFontDataPatch);
  }
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX) && defined(USE_NSS)
  // On platforms where we use system NSS shared libraries,
  // initialize NSS now because it won't be able to load the .so's
  // after we engage the sandbox.
  if (!command_line.HasSwitch(switches::kSingleProcess))
    crypto::InitNSSSafely();
#elif defined(OS_WIN)
  // crypt32.dll is used to decode X509 certificates for Chromoting.
  // Only load this library when the feature is enabled.
  std::string error;
  base::LoadNativeLibrary(base::FilePath(L"crypt32.dll"), &error);
#endif
  // Setup initial set of crash dump data for Field Trials in this renderer.
  chrome_variations::SetChildProcessLoggingVariationList();
}

ChromeRenderProcessObserver::~ChromeRenderProcessObserver() {
}

bool ChromeRenderProcessObserver::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeRenderProcessObserver, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetIsIncognitoProcess,
                        OnSetIsIncognitoProcess)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetExtensionActivityLogEnabled,
                        OnSetExtensionActivityLogEnabled)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetCacheCapacities, OnSetCacheCapacities)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_ClearCache, OnClearCache)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetFieldTrialGroup, OnSetFieldTrialGroup)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_GetV8HeapStats, OnGetV8HeapStats)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_GetCacheResourceStats,
                        OnGetCacheResourceStats)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_PurgeMemory, OnPurgeMemory)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetContentSettingRules,
                        OnSetContentSettingRules)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ChromeRenderProcessObserver::OnSetIsIncognitoProcess(
    bool is_incognito_process) {
  is_incognito_process_ = is_incognito_process;
}

void ChromeRenderProcessObserver::OnSetExtensionActivityLogEnabled(
    bool extension_activity_log_enabled) {
  extension_activity_log_enabled_ = extension_activity_log_enabled;
}

void ChromeRenderProcessObserver::OnSetContentSettingRules(
    const RendererContentSettingRules& rules) {
  content_setting_rules_ = rules;
}

void ChromeRenderProcessObserver::OnSetCacheCapacities(size_t min_dead_capacity,
                                                       size_t max_dead_capacity,
                                                       size_t capacity) {
  WebCache::setCapacities(
      min_dead_capacity, max_dead_capacity, capacity);
}

void ChromeRenderProcessObserver::OnClearCache(bool on_navigation) {
  if (on_navigation) {
    clear_cache_pending_ = true;
  } else {
    WebCache::clear();
  }
}

void ChromeRenderProcessObserver::OnGetCacheResourceStats() {
  WebCache::ResourceTypeStats stats;
  WebCache::getResourceTypeStats(&stats);
  RenderThread::Get()->Send(new ChromeViewHostMsg_ResourceTypeStats(stats));
}

void ChromeRenderProcessObserver::OnSetFieldTrialGroup(
    const std::string& field_trial_name,
    const std::string& group_name) {
  base::FieldTrial* trial =
      base::FieldTrialList::CreateFieldTrial(field_trial_name, group_name);
  // Ensure the trial is marked as "used" by calling group() on it. This is
  // needed to ensure the trial is properly reported in renderer crash reports.
  trial->group();
  chrome_variations::SetChildProcessLoggingVariationList();
}

void ChromeRenderProcessObserver::OnGetV8HeapStats() {
  v8::HeapStatistics heap_stats;
  // TODO(svenpanne) The call below doesn't take web workers into account, this
  // has to be done manually by iterating over all Isolates involved.
  v8::Isolate::GetCurrent()->GetHeapStatistics(&heap_stats);
  RenderThread::Get()->Send(new ChromeViewHostMsg_V8HeapStats(
      heap_stats.total_heap_size(), heap_stats.used_heap_size()));
}

void ChromeRenderProcessObserver::OnPurgeMemory() {
  RenderThread::Get()->EnsureWebKitInitialized();

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

  v8::V8::LowMemoryNotification();

  // Tell our allocator to release any free pages it's still holding.
  base::allocator::ReleaseFreeMemory();

  if (client_)
    client_->OnPurgeMemory();
}

void ChromeRenderProcessObserver::ExecutePendingClearCache() {
  if (clear_cache_pending_) {
    clear_cache_pending_ = false;
    WebCache::clear();
  }
}

const RendererContentSettingRules*
ChromeRenderProcessObserver::content_setting_rules() const {
  return &content_setting_rules_;
}
