// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_render_process_observer.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/threading/platform_thread.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_localization_peer.h"
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
#include "media/base/media.h"
#include "media/base/media_switches.h"
#include "net/base/net_errors.h"
#include "net/base/net_module.h"
#include "third_party/sqlite/sqlite3.h"
#include "third_party/tcmalloc/chromium/src/google/heap-profiler.h"
#include "third_party/tcmalloc/chromium/src/google/malloc_extension.h"
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

static const unsigned int kCacheStatsDelayMS = 2000 /* milliseconds */;

class RendererResourceDelegate : public content::ResourceDispatcherDelegate {
 public:
  RendererResourceDelegate()
      : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  }

  virtual webkit_glue::ResourceLoaderBridge::Peer* OnRequestComplete(
      webkit_glue::ResourceLoaderBridge::Peer* current_peer,
      ResourceType::Type resource_type,
      const net::URLRequestStatus& status) {
    // Update the browser about our cache.
    // Rate limit informing the host of our cache stats.
    if (!weak_factory_.HasWeakPtrs()) {
      MessageLoop::current()->PostDelayedTask(
         FROM_HERE,
         base::Bind(&RendererResourceDelegate::InformHostOfCacheStats,
                    weak_factory_.GetWeakPtr()),
         kCacheStatsDelayMS);
    }

    if (status.status() != net::URLRequestStatus::CANCELED ||
        status.error() == net::ERR_ABORTED) {
      return NULL;
    }

    // Resource canceled with a specific error are filtered.
    return SecurityFilterPeer::CreateSecurityFilterPeerForDeniedRequest(
        resource_type, current_peer, status.error());
  }

  virtual webkit_glue::ResourceLoaderBridge::Peer* OnReceivedResponse(
      webkit_glue::ResourceLoaderBridge::Peer* current_peer,
      const std::string& mime_type,
      const GURL& url) {
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

#if defined(OS_POSIX)
class SuicideOnChannelErrorFilter : public IPC::ChannelProxy::MessageFilter {
  void OnChannelError() {
    // On POSIX, at least, one can install an unload handler which loops
    // forever and leave behind a renderer process which eats 100% CPU forever.
    //
    // This is because the terminate signals (ViewMsg_ShouldClose and the error
    // from the IPC channel) are routed to the main message loop but never
    // processed (because that message loop is stuck in V8).
    //
    // One could make the browser SIGKILL the renderers, but that leaves open a
    // large window where a browser failure (or a user, manually terminating
    // the browser because "it's stuck") will leave behind a process eating all
    // the CPU.
    //
    // So, we install a filter on the channel so that we can process this event
    // here and kill the process.

    _exit(0);
  }
};
#endif  // OS_POSIX

}  // namespace

bool ChromeRenderProcessObserver::is_incognito_process_ = false;

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

  RenderThread* thread = RenderThread::Get();
  resource_delegate_.reset(new RendererResourceDelegate());
  thread->SetResourceDispatcherDelegate(resource_delegate_.get());

#if defined(OS_POSIX)
  thread->AddFilter(new SuicideOnChannelErrorFilter());
#endif

  // Configure modules that need access to resources.
  net::NetModule::SetResourceProvider(chrome_common_net::NetResourceProvider);

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

#if defined(OS_POSIX) && !defined(OS_MACOSX) && defined(USE_NSS)
  // Remoting requires NSS to function properly.
  if (!command_line.HasSwitch(switches::kSingleProcess)) {
    // We are going to fork to engage the sandbox and we have not loaded
    // any security modules so it is safe to disable the fork check in NSS.
    crypto::DisableNSSForkCheck();
    crypto::ForceNSSNoDBInit();
    crypto::EnsureNSSInit();
  }
#elif defined(OS_WIN)
  // crypt32.dll is used to decode X509 certificates for Chromoting.
  // Only load this library when the feature is enabled.
  std::string error;
  base::LoadNativeLibrary(FilePath(L"crypt32.dll"), &error);
#endif

  // Note that under Linux, the media library will normally already have
  // been initialized by the Zygote before this instance became a Renderer.
  FilePath media_path;
  PathService::Get(chrome::DIR_MEDIA_LIBS, &media_path);
  if (!media_path.empty())
    media::InitializeMediaLibrary(media_path);
}

ChromeRenderProcessObserver::~ChromeRenderProcessObserver() {
}

bool ChromeRenderProcessObserver::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeRenderProcessObserver, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetIsIncognitoProcess,
                        OnSetIsIncognitoProcess)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetCacheCapacities, OnSetCacheCapacities)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_ClearCache, OnClearCache)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetFieldTrialGroup, OnSetFieldTrialGroup)
#if defined(USE_TCMALLOC)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_GetRendererTcmalloc,
                        OnGetRendererTcmalloc)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetTcmallocHeapProfiling,
                        OnSetTcmallocHeapProfiling)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_WriteTcmallocHeapProfile,
                        OnWriteTcmallocHeapProfile)
#endif
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

void ChromeRenderProcessObserver::WebKitInitialized() {
  WebRuntimeFeatures::enableMediaPlayer(media::IsMediaLibraryInitialized());
}

void ChromeRenderProcessObserver::OnSetIsIncognitoProcess(
    bool is_incognito_process) {
  is_incognito_process_ = is_incognito_process;
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

#if defined(USE_TCMALLOC)
void ChromeRenderProcessObserver::OnGetRendererTcmalloc() {
  std::string result;
  char buffer[1024 * 32];
  MallocExtension::instance()->GetStats(buffer, sizeof(buffer));
  result.append(buffer);
  RenderThread::Get()->Send(new ChromeViewHostMsg_RendererTcmalloc(result));
}

void ChromeRenderProcessObserver::OnSetTcmallocHeapProfiling(
    bool profiling, const std::string& filename_prefix) {
#if !defined(OS_WIN)
  // TODO(stevenjb): Create MallocExtension wrappers for HeapProfile functions.
  if (profiling)
    HeapProfilerStart(filename_prefix.c_str());
  else
    HeapProfilerStop();
#endif
}

void ChromeRenderProcessObserver::OnWriteTcmallocHeapProfile(
    const FilePath::StringType& filename) {
#if !defined(OS_WIN)
  // TODO(stevenjb): Create MallocExtension wrappers for HeapProfile functions.
  if (!IsHeapProfilerRunning())
    return;
  char* profile = GetHeapProfile();
  if (!profile) {
    LOG(WARNING) << "Unable to get heap profile.";
    return;
  }
  // The render process can not write to a file, so copy the result into
  // a string and pass it to the handler (which runs on the browser host).
  std::string result(profile);
  delete profile;
  RenderThread::Get()->Send(
      new ChromeViewHostMsg_WriteTcmallocHeapProfile_ACK(filename, result));
#endif
}

#endif

void ChromeRenderProcessObserver::OnSetFieldTrialGroup(
    const std::string& field_trial_name,
    const std::string& group_name) {
  base::FieldTrialList::CreateFieldTrial(field_trial_name, group_name);
}

void ChromeRenderProcessObserver::OnGetV8HeapStats() {
  v8::HeapStatistics heap_stats;
  v8::V8::GetHeapStatistics(&heap_stats);
  RenderThread::Get()->Send(new ChromeViewHostMsg_V8HeapStats(
      heap_stats.total_heap_size(), heap_stats.used_heap_size()));
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

  v8::V8::LowMemoryNotification();

#if !defined(OS_MACOSX) && defined(USE_TCMALLOC)
  // Tell tcmalloc to release any free pages it's still holding.
  MallocExtension::instance()->ReleaseFreeMemory();
#endif

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
