// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_render_process_observer.h"

#include "base/command_line.h"
#include "base/file_util.h"
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
#include "content/common/resource_dispatcher.h"
#include "content/common/resource_dispatcher_delegate.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_thread.h"
#include "content/renderer/render_view.h"
#include "content/renderer/render_view_visitor.h"
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

#if defined(OS_MACOSX)
#include "base/eintr_wrapper.h"
#include "chrome/app/breakpad_mac.h"
#endif

using WebKit::WebCache;
using WebKit::WebCrossOriginPreflightResultCache;
using WebKit::WebFontCache;
using WebKit::WebRuntimeFeatures;

namespace {

static const unsigned int kCacheStatsDelayMS = 2000 /* milliseconds */;

class RendererResourceDelegate : public ResourceDispatcherDelegate {
 public:
  RendererResourceDelegate()
      : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  }

  virtual webkit_glue::ResourceLoaderBridge::Peer* OnRequestComplete(
      webkit_glue::ResourceLoaderBridge::Peer* current_peer,
      ResourceType::Type resource_type,
      const net::URLRequestStatus& status) {
    // Update the browser about our cache.
    // Rate limit informing the host of our cache stats.
    if (method_factory_.empty()) {
      MessageLoop::current()->PostDelayedTask(
         FROM_HERE,
         method_factory_.NewRunnableMethod(
             &RendererResourceDelegate::InformHostOfCacheStats),
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
        current_peer, RenderThread::current(), mime_type, url);
  }

 private:
  void InformHostOfCacheStats() {
    WebCache::UsageStats stats;
    WebCache::getUsageStats(&stats);
    RenderThread::current()->Send(new ChromeViewHostMsg_UpdatedCacheStats(
        stats));
  }

  ScopedRunnableMethodFactory<RendererResourceDelegate> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(RendererResourceDelegate);
};

class RenderViewContentSettingsSetter : public RenderViewVisitor {
 public:
  RenderViewContentSettingsSetter(const GURL& url,
                                  const ContentSettings& content_settings)
      : url_(url),
        content_settings_(content_settings) {
  }

  virtual bool Visit(RenderView* render_view) {
    if (GURL(render_view->webview()->mainFrame()->document().url()) == url_) {
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
      if (RenderThread::current()->Send(new ViewHostMsg_PreCacheFont(logfont)))
        rv = GetFontData(hdc, table, offset, buffer, length);
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

#if defined(OS_MACOSX)
    // TODO(viettrungluu): crbug.com/28547: The following is needed, as a
    // stopgap, to avoid leaking due to not releasing Breakpad properly.
    // TODO(viettrungluu): Investigate why this is being called.
    if (IsCrashReporterEnabled()) {
      VLOG(1) << "Cleaning up Breakpad.";
      DestructCrashReporter();
    } else {
      VLOG(1) << "Breakpad not enabled; no clean-up needed.";
    }
#endif  // OS_MACOSX

    _exit(0);
  }
};
#endif  // OS_POSIX

#if defined(OS_MACOSX)
// TODO(viettrungluu): crbug.com/28547: The following signal handling is needed,
// as a stopgap, to avoid leaking due to not releasing Breakpad properly.
// Without this problem, this could all be eliminated. Remove when Breakpad is
// fixed?
// TODO(viettrungluu): Code taken from browser_main.cc (with a bit of editing).
// The code should be properly shared (or this code should be eliminated).
int g_shutdown_pipe_write_fd = -1;

void SIGTERMHandler(int signal) {
  RAW_CHECK(signal == SIGTERM);

  // Reinstall the default handler.  We had one shot at graceful shutdown.
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_DFL;
  CHECK(sigaction(signal, &action, NULL) == 0);

  RAW_CHECK(g_shutdown_pipe_write_fd != -1);
  size_t bytes_written = 0;
  do {
    int rv = HANDLE_EINTR(
        write(g_shutdown_pipe_write_fd,
              reinterpret_cast<const char*>(&signal) + bytes_written,
              sizeof(signal) - bytes_written));
    RAW_CHECK(rv >= 0);
    bytes_written += rv;
  } while (bytes_written < sizeof(signal));
}

class ShutdownDetector : public base::PlatformThread::Delegate {
 public:
  explicit ShutdownDetector(int shutdown_fd) : shutdown_fd_(shutdown_fd) {
    CHECK(shutdown_fd_ != -1);
  }

  virtual void ThreadMain() {
    int signal;
    size_t bytes_read = 0;
    ssize_t ret;
    do {
      ret = HANDLE_EINTR(
          read(shutdown_fd_,
               reinterpret_cast<char*>(&signal) + bytes_read,
               sizeof(signal) - bytes_read));
      if (ret < 0) {
        NOTREACHED() << "Unexpected error: " << strerror(errno);
        break;
      } else if (ret == 0) {
        NOTREACHED() << "Unexpected closure of shutdown pipe.";
        break;
      }
      bytes_read += ret;
    } while (bytes_read < sizeof(signal));

    if (bytes_read == sizeof(signal))
      VLOG(1) << "Handling shutdown for signal " << signal << ".";
    else
      VLOG(1) << "Handling shutdown for unknown signal.";

    // Clean up Breakpad if necessary.
    if (IsCrashReporterEnabled()) {
      VLOG(1) << "Cleaning up Breakpad.";
      DestructCrashReporter();
    } else {
      VLOG(1) << "Breakpad not enabled; no clean-up needed.";
    }

    // Something went seriously wrong, so get out.
    if (bytes_read != sizeof(signal)) {
      LOG(WARNING) << "Failed to get signal. Quitting ungracefully.";
      _exit(1);
    }

    // Re-raise the signal.
    kill(getpid(), signal);

    // The signal may be handled on another thread. Give that a chance to
    // happen.
    sleep(3);

    // We really should be dead by now.  For whatever reason, we're not. Exit
    // immediately, with the exit status set to the signal number with bit 8
    // set.  On the systems that we care about, this exit status is what is
    // normally used to indicate an exit by this signal's default handler.
    // This mechanism isn't a de jure standard, but even in the worst case, it
    // should at least result in an immediate exit.
    LOG(WARNING) << "Still here, exiting really ungracefully.";
    _exit(signal | (1 << 7));
  }

 private:
  const int shutdown_fd_;

  DISALLOW_COPY_AND_ASSIGN(ShutdownDetector);
};
#endif  // OS_MACOSX

}  // namespace

bool ChromeRenderProcessObserver::is_incognito_process_ = false;

ChromeRenderProcessObserver::ChromeRenderProcessObserver(
    chrome::ChromeContentRendererClient* client)
    : client_(client) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kEnableWatchdog)) {
    // TODO(JAR): Need to implement renderer IO msgloop watchdog.
  }

  if (command_line.HasSwitch(switches::kDumpHistogramsOnExit)) {
    base::StatisticsRecorder::set_dump_on_exit(true);
  }

  RenderThread* thread = RenderThread::current();
  resource_delegate_.reset(new RendererResourceDelegate());
  thread->resource_dispatcher()->set_delegate(resource_delegate_.get());

#if defined(OS_POSIX)
  thread->AddFilter(new SuicideOnChannelErrorFilter());
#endif

#if defined(OS_MACOSX)
  // TODO(viettrungluu): Code taken from browser_main.cc.
  int pipefd[2];
  int ret = pipe(pipefd);
  if (ret < 0) {
    PLOG(DFATAL) << "Failed to create pipe";
  } else {
    int shutdown_pipe_read_fd = pipefd[0];
    g_shutdown_pipe_write_fd = pipefd[1];
    const size_t kShutdownDetectorThreadStackSize = 4096;
    if (!base::PlatformThread::CreateNonJoinable(
            kShutdownDetectorThreadStackSize,
            new ShutdownDetector(shutdown_pipe_read_fd))) {
      LOG(DFATAL) << "Failed to create shutdown detector task.";
    }
  }

  // crbug.com/28547: When Breakpad is in use, handle SIGTERM to avoid leaking
  // Mach ports.
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIGTERMHandler;
  CHECK(sigaction(SIGTERM, &action, NULL) == 0);
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
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetDefaultContentSettings,
                        OnSetDefaultContentSettings)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetContentSettingsForCurrentURL,
                        OnSetContentSettingsForCurrentURL)
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

void ChromeRenderProcessObserver::OnSetContentSettingsForCurrentURL(
    const GURL& url,
    const ContentSettings& content_settings) {
  RenderViewContentSettingsSetter setter(url, content_settings);
  RenderView::ForEach(&setter);
}

void ChromeRenderProcessObserver::OnSetDefaultContentSettings(
    const ContentSettings& content_settings) {
  ContentSettingsObserver::SetDefaultContentSettings(content_settings);
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
  Send(new ChromeViewHostMsg_ResourceTypeStats(stats));
}

#if defined(USE_TCMALLOC)
void ChromeRenderProcessObserver::OnGetRendererTcmalloc() {
  std::string result;
  char buffer[1024 * 32];
  MallocExtension::instance()->GetStats(buffer, sizeof(buffer));
  result.append(buffer);
  Send(new ChromeViewHostMsg_RendererTcmalloc(result));
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
  Send(new ChromeViewHostMsg_WriteTcmallocHeapProfile_ACK(filename, result));
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
  Send(new ChromeViewHostMsg_V8HeapStats(heap_stats.total_heap_size(),
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

#if !defined(OS_MACOSX) && defined(USE_TCMALLOC)
  // Tell tcmalloc to release any free pages it's still holding.
  MallocExtension::instance()->ReleaseFreeMemory();
#endif

  if (client_)
    client_->OnPurgeMemory();
}
