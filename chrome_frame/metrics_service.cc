// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//------------------------------------------------------------------------------
// Description of the life cycle of a instance of MetricsService.
//
//  OVERVIEW
//
// A MetricsService instance is created at ChromeFrame startup in
// the IE process. It is the central controller for the UMA log data.
// Its major job is to manage logs, prepare them for transmission.
// Currently only histogram data is tracked in log.  When MetricsService
// prepares log for submission it snapshots the current stats of histograms,
// translates log to XML.  Transmission includes submitting a compressed log
// as data in a URL-get, and is performed using functionality provided by
// Urlmon
// The actual transmission is performed using a windows timer procedure which
// basically means that the thread on which the MetricsService object is
// instantiated needs a message pump. Also on IE7 where every tab is created
// on its own thread we would have a case where the timer procedures can
// compete for sending histograms.
//
// When preparing log for submission we acquire a list of all local histograms
// that have been flagged for upload to the UMA server.
//
// When ChromeFrame shuts down, there will typically be a fragment of an ongoing
// log that has not yet been transmitted.  Currently this data is ignored.
//
// With the above overview, we can now describe the state machine's various
// stats, based on the State enum specified in the state_ member.  Those states
// are:
//
//    INITIALIZED,      // Constructor was called.
//    ACTIVE,           // Accumalating log data.
//    STOPPED,          // Service has stopped.
//
//-----------------------------------------------------------------------------

#include "chrome_frame/metrics_service.h"

#include <atlbase.h>
#include <atlwin.h>
#include <objbase.h>
#include <windows.h>

#include <vector>

#if defined(USE_SYSTEM_LIBBZ2)
#include <bzlib.h>
#else
#include "third_party/bzip2/bzlib.h"
#endif

#include "base/file_version_info.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/string_number_conversions.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/net/url_fetcher.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/chrome_frame_distribution.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome_frame/bind_status_callback_impl.h"
#include "chrome_frame/chrome_frame_delegate.h"
#include "chrome_frame/crash_reporting/crash_metrics.h"
#include "chrome_frame/html_utils.h"
#include "chrome_frame/http_negotiate.h"
#include "chrome_frame/utils.h"
#include "net/base/capturing_net_log.h"
#include "net/base/cert_verifier.h"
#include "net/base/host_resolver.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/base/upload_data.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_status.h"

using base::Time;
using base::TimeDelta;

static const char kMetricsType[] = "application/vnd.mozilla.metrics.bz2";

// The first UMA upload occurs after this interval.
static const int kInitialUMAUploadTimeoutMilliSeconds = 30000;

// Default to one UMA upload per 10 mins.
static const int kMinMilliSecondsPerUMAUpload = 600000;

base::LazyInstance<MetricsService>
    g_metrics_instance_(base::LINKER_INITIALIZED);

base::Lock MetricsService::metrics_service_lock_;

// Traits to create an instance of the ChromeFrame upload thread.
struct UploadThreadInstanceTraits
    : public base::LeakyLazyInstanceTraits<base::Thread> {
  static base::Thread* New(void* instance) {
    // Use placement new to initialize our instance in our preallocated space.
    // The parenthesis is very important here to force POD type initialization.
    base::Thread* upload_thread =
        new(instance) base::Thread("ChromeFrameUploadThread");
    base::Thread::Options options;
    options.message_loop_type = MessageLoop::TYPE_IO;
    bool ret = upload_thread->StartWithOptions(options);
    if (!ret) {
      NOTREACHED() << "Failed to start upload thread";
    }
    return upload_thread;
  }
};

// ChromeFrame UMA uploads occur on this thread. This thread is started on the
// IE UI thread. This thread needs to be stopped on the same thread it was
// started on. We don't have a good way of achieving this at this point. This
// thread object is currently leaked.
// TODO(ananta)
// TODO(vitalybuka@chromium.org) : Fix this by using MetricsService::Stop() in
// appropriate location.
base::LazyInstance<base::Thread, UploadThreadInstanceTraits>
    g_metrics_upload_thread_(base::LINKER_INITIALIZED);

base::Lock g_metrics_service_lock;

extern base::LazyInstance<base::StatisticsRecorder> g_statistics_recorder_;

// This class provides HTTP request context information for metrics upload
// requests initiated by ChromeFrame.
class ChromeFrameUploadRequestContext : public net::URLRequestContext {
 public:
  explicit ChromeFrameUploadRequestContext(MessageLoop* io_loop)
      : io_loop_(io_loop) {
    Initialize();
  }

  ~ChromeFrameUploadRequestContext() {
    DVLOG(1) << __FUNCTION__;
    delete http_transaction_factory_;
    delete http_auth_handler_factory_;
    delete cert_verifier_;
    delete host_resolver_;
  }

  void Initialize() {
    user_agent_ = http_utils::GetDefaultUserAgent();
    user_agent_ = http_utils::AddChromeFrameToUserAgentValue(
      user_agent_);

    host_resolver_ =
        net::CreateSystemHostResolver(net::HostResolver::kDefaultParallelism,
                                      NULL, NULL);
    cert_verifier_ = new net::CertVerifier;
    net::ProxyConfigService* proxy_config_service =
        net::ProxyService::CreateSystemProxyConfigService(NULL, NULL);
    DCHECK(proxy_config_service);

    proxy_service_ = net::ProxyService::CreateUsingSystemProxyResolver(
        proxy_config_service, 0, NULL);
    DCHECK(proxy_service_);

    ssl_config_service_ = new net::SSLConfigServiceDefaults;

    url_security_manager_.reset(
        net::URLSecurityManager::Create(NULL, NULL));

    std::string csv_auth_schemes = "basic,digest,ntlm,negotiate";
    std::vector<std::string> supported_schemes;
    base::SplitString(csv_auth_schemes, ',', &supported_schemes);

    http_auth_handler_factory_ = net::HttpAuthHandlerRegistryFactory::Create(
        supported_schemes, url_security_manager_.get(), host_resolver_,
        std::string(), false, false);

    http_transaction_factory_ = new net::HttpCache(
        net::HttpNetworkLayer::CreateFactory(host_resolver_,
                                             cert_verifier_,
                                             NULL /* dnsrr_resovler */,
                                             NULL /* dns_cert_checker*/,
                                             NULL /* ssl_host_info */,
                                             proxy_service_,
                                             ssl_config_service_,
                                             http_auth_handler_factory_,
                                             network_delegate_,
                                             NULL),
        NULL /* net_log */,
        net::HttpCache::DefaultBackend::InMemory(0));
  }

  virtual const std::string& GetUserAgent(const GURL& url) const {
    return user_agent_;
  }

 private:
  std::string user_agent_;
  MessageLoop* io_loop_;
  scoped_ptr<net::URLSecurityManager> url_security_manager_;
};

// This class provides an interface to retrieve the URL request context for
// metrics HTTP upload requests initiated by ChromeFrame.
class ChromeFrameUploadRequestContextGetter : public URLRequestContextGetter {
 public:
  explicit ChromeFrameUploadRequestContextGetter(MessageLoop* io_loop)
      : io_loop_(io_loop) {}

  virtual net::URLRequestContext* GetURLRequestContext() {
    if (!context_)
      context_ = new ChromeFrameUploadRequestContext(io_loop_);
    return context_;
  }

  virtual scoped_refptr<base::MessageLoopProxy> GetIOMessageLoopProxy() const {
    if (!io_message_loop_proxy_.get()) {
      io_message_loop_proxy_ = base::MessageLoopProxy::CreateForCurrentThread();
    }
    return io_message_loop_proxy_;
  }

 private:
  ~ChromeFrameUploadRequestContextGetter() {
    DVLOG(1) << __FUNCTION__;
  }

  scoped_refptr<net::URLRequestContext> context_;
  mutable scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;
  MessageLoop* io_loop_;
};

// This class provides functionality to upload the ChromeFrame UMA data to the
// server. An instance of this class is created whenever we have data to be
// uploaded to the server.
class ChromeFrameMetricsDataUploader
    : public URLFetcher::Delegate,
      public base::RefCountedThreadSafe<ChromeFrameMetricsDataUploader>,
      public CWindowImpl<ChromeFrameMetricsDataUploader>,
      public TaskMarshallerThroughWindowsMessages<
          ChromeFrameMetricsDataUploader> {
 public:
  BEGIN_MSG_MAP(ChromeFrameMetricsDataUploader)
    CHAIN_MSG_MAP(
        TaskMarshallerThroughWindowsMessages<ChromeFrameMetricsDataUploader>)
  END_MSG_MAP()

  ChromeFrameMetricsDataUploader()
      : fetcher_(NULL) {
    DVLOG(1) << __FUNCTION__;
    creator_thread_id_ = base::PlatformThread::CurrentId();
  }

  ~ChromeFrameMetricsDataUploader() {
    DVLOG(1) << __FUNCTION__;
    DCHECK(creator_thread_id_ == base::PlatformThread::CurrentId());
  }

  virtual void OnFinalMessage(HWND wnd) {
    Release();
  }

  bool Initialize() {
    bool ret = false;

    if (!Create(NULL, NULL, NULL, WS_OVERLAPPEDWINDOW)) {
      NOTREACHED() << "Failed to create window";
      return ret;
    }
    DCHECK(IsWindow());

    if (!g_metrics_upload_thread_.Get().IsRunning()) {
      NOTREACHED() << "Upload thread is not running";
      return ret;
    }

    ret = true;
    // Grab a reference to the current instance which ensures that it stays
    // around until the HTTP request initiated below completes.
    // Corresponding Release is in OnFinalMessage.
    AddRef();
    return ret;
  }

  bool Uninitialize() {
    DestroyWindow();
    return true;
  }

  static HRESULT ChromeFrameMetricsDataUploader::UploadDataHelper(
      const std::string& upload_data) {
    scoped_refptr<ChromeFrameMetricsDataUploader> data_uploader =
        new ChromeFrameMetricsDataUploader();

    if (!data_uploader->Initialize()) {
      NOTREACHED() << "Failed to initialize ChromeFrameMetricsDataUploader";
      return E_FAIL;
    }

    MessageLoop* io_loop = g_metrics_upload_thread_.Get().message_loop();
    if (!io_loop) {
      NOTREACHED() << "Failed to initialize ChromeFrame UMA upload thread";
      return E_FAIL;
    }

    io_loop->PostTask(
        FROM_HERE,
        NewRunnableMethod(data_uploader.get(),
                          &ChromeFrameMetricsDataUploader::UploadData,
                          upload_data, io_loop));
    return S_OK;
  }

  void UploadData(const std::string& upload_data, MessageLoop* message_loop) {
    DCHECK(fetcher_ == NULL);
    DCHECK(message_loop != NULL);

    BrowserDistribution* dist = ChromeFrameDistribution::GetDistribution();
    DCHECK(dist != NULL);

    fetcher_ = new URLFetcher(GURL(WideToUTF8(dist->GetStatsServerURL())),
                              URLFetcher::POST, this);

    fetcher_->set_request_context(new ChromeFrameUploadRequestContextGetter(
        message_loop));
    fetcher_->set_upload_data(kMetricsType, upload_data);
    fetcher_->Start();
  }

  // URLFetcher::Delegate
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data) {
    DVLOG(1) << __FUNCTION__ << base::StringPrintf(
        ": url : %hs, status:%d, response code: %d\n", url.spec().c_str(),
        status.status(), response_code);
    delete fetcher_;
    fetcher_ = NULL;

    PostTask(FROM_HERE,
             NewRunnableMethod(this,
                               &ChromeFrameMetricsDataUploader::Uninitialize));
  }

 private:
  URLFetcher* fetcher_;
  base::PlatformThreadId creator_thread_id_;
};

MetricsService* MetricsService::GetInstance() {
  base::AutoLock lock(g_metrics_service_lock);
  return &g_metrics_instance_.Get();
}

MetricsService::MetricsService()
    : recording_active_(false),
      reporting_active_(false),
      user_permits_upload_(false),
      state_(INITIALIZED),
      thread_(NULL),
      initial_uma_upload_(true),
      transmission_timer_id_(0) {
}

MetricsService::~MetricsService() {
  SetRecording(false);
  if (pending_log_) {
    delete pending_log_;
    pending_log_ = NULL;
  }
  if (current_log_) {
    delete current_log_;
    current_log_ = NULL;
  }
}

void MetricsService::InitializeMetricsState() {
  DCHECK(state_ == INITIALIZED);

  thread_ = base::PlatformThread::CurrentId();

  user_permits_upload_ = GoogleUpdateSettings::GetCollectStatsConsent();
  // Update session ID
  session_id_ = CrashMetricsReporter::GetInstance()->IncrementMetric(
      CrashMetricsReporter::SESSION_ID);

  // Ensure that an instance of the StatisticsRecorder object is created.
  g_statistics_recorder_.Get();

  if (user_permits_upload_) {
    // Ensure that an instance of the metrics upload thread is created.
    g_metrics_upload_thread_.Get();
  }

  CrashMetricsReporter::GetInstance()->set_active(true);
}

// static
void MetricsService::Start() {
  base::AutoLock lock(metrics_service_lock_);

  if (GetInstance()->state_ == ACTIVE)
    return;

  GetInstance()->InitializeMetricsState();
  GetInstance()->SetRecording(true);
  GetInstance()->SetReporting(true);
}

// static
void MetricsService::Stop() {
  {
    base::AutoLock lock(metrics_service_lock_);

    GetInstance()->SetReporting(false);
    GetInstance()->SetRecording(false);
  }

  if (GetInstance()->user_permits_upload_)
    g_metrics_upload_thread_.Get().Stop();
}

void MetricsService::SetRecording(bool enabled) {
  if (enabled == recording_active_)
    return;

  if (enabled) {
    if (client_id_.empty()) {
      client_id_ = GenerateClientID();
      // Save client id somewhere.
    }
    StartRecording();

  } else {
    state_ = STOPPED;
  }
  recording_active_ = enabled;
}

// static
std::string MetricsService::GenerateClientID() {
  const int kGUIDSize = 39;

  GUID guid;
  HRESULT guid_result = CoCreateGuid(&guid);
  DCHECK(SUCCEEDED(guid_result));

  std::wstring guid_string;
  int result = StringFromGUID2(guid,
                               WriteInto(&guid_string, kGUIDSize), kGUIDSize);
  DCHECK(result == kGUIDSize);
  return WideToUTF8(guid_string.substr(1, guid_string.length() - 2));
}

// static
void CALLBACK MetricsService::TransmissionTimerProc(HWND window,
                                                    unsigned int message,
                                                    unsigned int event_id,
                                                    unsigned int time) {
  DVLOG(1) << "Transmission timer notified";
  DCHECK(GetInstance() != NULL);
  GetInstance()->UploadData();
  if (GetInstance()->initial_uma_upload_) {
    // If this is the first uma upload by this process then subsequent uma
    // uploads should occur once every 10 minutes(default).
    GetInstance()->initial_uma_upload_ = false;
    DCHECK(GetInstance()->transmission_timer_id_ != 0);
    SetTimer(NULL, GetInstance()->transmission_timer_id_,
             kMinMilliSecondsPerUMAUpload,
             reinterpret_cast<TIMERPROC>(TransmissionTimerProc));
  }
}

void MetricsService::SetReporting(bool enable) {
  static const int kChromeFrameMetricsTimerId = 0xFFFFFFFF;

  DCHECK_EQ(thread_, base::PlatformThread::CurrentId());
  if (reporting_active_ != enable) {
    reporting_active_ = enable;
    if (reporting_active_) {
      transmission_timer_id_ =
          SetTimer(NULL, kChromeFrameMetricsTimerId,
                   kInitialUMAUploadTimeoutMilliSeconds,
                   reinterpret_cast<TIMERPROC>(TransmissionTimerProc));
    } else {
      UploadData();
    }
  }
}

//------------------------------------------------------------------------------
// Recording control methods

void MetricsService::StartRecording() {
  DCHECK_EQ(thread_, base::PlatformThread::CurrentId());
  if (current_log_)
    return;

  current_log_ = new MetricsLogBase(client_id_, session_id_,
                                    GetVersionString());
  if (state_ == INITIALIZED)
    state_ = ACTIVE;
}

void MetricsService::StopRecording(bool save_log) {
  DCHECK_EQ(thread_, base::PlatformThread::CurrentId());
  if (!current_log_)
    return;

  // Put incremental histogram deltas at the end of all log transmissions.
  // Don't bother if we're going to discard current_log_.
  if (save_log) {
    CrashMetricsReporter::GetInstance()->RecordCrashMetrics();
    RecordCurrentHistograms();
  }

  if (save_log) {
    pending_log_ = current_log_;
  }
  current_log_ = NULL;
}

void MetricsService::MakePendingLog() {
  DCHECK_EQ(thread_, base::PlatformThread::CurrentId());
  if (pending_log())
    return;

  switch (state_) {
    case INITIALIZED:  // We should be further along by now.
      DCHECK(false);
      return;

    case ACTIVE:
      StopRecording(true);
      StartRecording();
      break;

    default:
      DCHECK(false);
      return;
  }

  DCHECK(pending_log());
}

bool MetricsService::TransmissionPermitted() const {
  // If the user forbids uploading that's their business, and we don't upload
  // anything.
  return user_permits_upload_;
}

std::string MetricsService::PrepareLogSubmissionString() {
  DCHECK_EQ(thread_, base::PlatformThread::CurrentId());

  MakePendingLog();
  DCHECK(pending_log());
  if (pending_log_== NULL) {
    return std::string();
  }

  pending_log_->CloseLog();
  std::string pending_log_text = pending_log_->GetEncodedLogString();
  DCHECK(!pending_log_text.empty());
  DiscardPendingLog();
  return pending_log_text;
}

bool MetricsService::UploadData() {
  DCHECK_EQ(thread_, base::PlatformThread::CurrentId());

  if (!GetInstance()->TransmissionPermitted())
    return false;

  static long currently_uploading = 0;
  if (InterlockedCompareExchange(&currently_uploading, 1, 0)) {
    DVLOG(1) << "Contention for uploading metrics data. Backing off";
    return false;
  }

  std::string pending_log_text = PrepareLogSubmissionString();
  DCHECK(!pending_log_text.empty());

  // Allow security conscious users to see all metrics logs that we send.
  VLOG(1) << "METRICS LOG: " << pending_log_text;

  bool ret = true;

  if (!Bzip2Compress(pending_log_text, &compressed_log_)) {
    NOTREACHED() << "Failed to compress log for transmission.";
    ret = false;
  } else {
    HRESULT hr = ChromeFrameMetricsDataUploader::UploadDataHelper(
        compressed_log_);
    DCHECK(SUCCEEDED(hr));
  }
  DiscardPendingLog();

  currently_uploading = 0;
  return ret;
}

// static
std::string MetricsService::GetVersionString() {
  chrome::VersionInfo version_info;
  if (version_info.is_valid()) {
    std::string version = version_info.Version();
    // Add the -F extensions to ensure that UMA data uploaded by ChromeFrame
    // lands in the ChromeFrame bucket.
    version += "-F";
    if (!version_info.IsOfficialBuild())
      version.append("-devel");
    return version;
  } else {
    NOTREACHED() << "Unable to retrieve version string.";
  }

  return std::string();
}
