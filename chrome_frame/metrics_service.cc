// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

#if defined(USE_SYSTEM_LIBBZ2)
#include <bzlib.h>
#else
#include "third_party/bzip2/bzlib.h"
#endif

#include "base/string16.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_comptr.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/metrics/metrics_log_base.h"
#include "chrome/common/metrics/metrics_log_manager.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome_frame/bind_status_callback_impl.h"
#include "chrome_frame/crash_reporting/crash_metrics.h"
#include "chrome_frame/html_utils.h"
#include "chrome_frame/utils.h"

using base::Time;
using base::TimeDelta;
using base::win::ScopedComPtr;

// The first UMA upload occurs after this interval.
static const int kInitialUMAUploadTimeoutMilliSeconds = 30000;

// Default to one UMA upload per 10 mins.
static const int kMinMilliSecondsPerUMAUpload = 600000;

base::LazyInstance<base::ThreadLocalPointer<MetricsService> >
    MetricsService::g_metrics_instance_ = LAZY_INSTANCE_INITIALIZER;

std::string MetricsService::client_id_;

base::Lock MetricsService::metrics_service_lock_;

// Initialize histogram statistics gathering system.
base::LazyInstance<base::StatisticsRecorder>
    g_statistics_recorder_ = LAZY_INSTANCE_INITIALIZER;

// This class provides functionality to upload the ChromeFrame UMA data to the
// server. An instance of this class is created whenever we have data to be
// uploaded to the server.
class ChromeFrameMetricsDataUploader : public BSCBImpl {
 public:
  ChromeFrameMetricsDataUploader()
      : cache_stream_(NULL),
        upload_data_size_(0) {
    DVLOG(1) << __FUNCTION__;
  }

  ~ChromeFrameMetricsDataUploader() {
    DVLOG(1) << __FUNCTION__;
  }

  static HRESULT ChromeFrameMetricsDataUploader::UploadDataHelper(
      const std::string& upload_data,
      const std::string& server_url,
      const std::string& mime_type) {
    CComObject<ChromeFrameMetricsDataUploader>* data_uploader = NULL;
    CComObject<ChromeFrameMetricsDataUploader>::CreateInstance(&data_uploader);
    DCHECK(data_uploader != NULL);

    data_uploader->AddRef();
    HRESULT hr = data_uploader->UploadData(upload_data, server_url, mime_type);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to initialize ChromeFrame UMA data uploader: Err"
                  << hr;
    }
    data_uploader->Release();
    return hr;
  }

  HRESULT UploadData(const std::string& upload_data,
                     const std::string& server_url,
                     const std::string& mime_type) {
    if (upload_data.empty()) {
      NOTREACHED() << "Invalid upload data";
      return E_INVALIDARG;
    }

    DCHECK(cache_stream_.get() == NULL);

    upload_data_size_ = upload_data.size() + 1;

    HRESULT hr = CreateStreamOnHGlobal(NULL, TRUE, cache_stream_.Receive());
    if (FAILED(hr)) {
      NOTREACHED() << "Failed to create stream. Error:"
                   << hr;
      return hr;
    }

    DCHECK(cache_stream_.get());

    unsigned long written = 0;
    cache_stream_->Write(upload_data.c_str(), upload_data_size_, &written);
    DCHECK(written == upload_data_size_);

    RewindStream(cache_stream_);

    server_url_ = ASCIIToWide(server_url);
    mime_type_ = mime_type;
    DCHECK(!server_url_.empty());
    DCHECK(!mime_type_.empty());

    hr = CreateURLMoniker(NULL, server_url_.c_str(),
                          upload_moniker_.Receive());
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to create url moniker for url:"
                  << server_url_.c_str()
                  << " Error:"
                  << hr;
    } else {
      ScopedComPtr<IBindCtx> context;
      hr = CreateAsyncBindCtx(0, this, NULL, context.Receive());
      DCHECK(SUCCEEDED(hr));
      DCHECK(context);

      ScopedComPtr<IStream> stream;
      hr = upload_moniker_->BindToStorage(
          context, NULL, IID_IStream,
          reinterpret_cast<void**>(stream.Receive()));
      if (FAILED(hr)) {
        NOTREACHED();
        DLOG(ERROR) << "Failed to bind to upload data moniker. Error:"
                    << hr;
      }
    }
    return hr;
  }

  STDMETHOD(BeginningTransaction)(LPCWSTR url, LPCWSTR headers, DWORD reserved,
                                  LPWSTR* additional_headers) {
    std::string new_headers;
    new_headers =
        StringPrintf("Content-Length: %s\r\n"
                     "Content-Type: %s\r\n"
                     "%s\r\n",
                     base::Int64ToString(upload_data_size_).c_str(),
                     mime_type_.c_str(),
                     http_utils::GetDefaultUserAgentHeaderWithCFTag().c_str());

    *additional_headers = reinterpret_cast<wchar_t*>(
        CoTaskMemAlloc((new_headers.size() + 1) * sizeof(wchar_t)));

    lstrcpynW(*additional_headers, ASCIIToWide(new_headers).c_str(),
              new_headers.size());

    return BSCBImpl::BeginningTransaction(url, headers, reserved,
                                          additional_headers);
  }

  STDMETHOD(GetBindInfo)(DWORD* bind_flags, BINDINFO* bind_info) {
    if ((bind_info == NULL) || (bind_info->cbSize == 0) ||
        (bind_flags == NULL))
      return E_INVALIDARG;

    *bind_flags = BINDF_ASYNCHRONOUS | BINDF_ASYNCSTORAGE | BINDF_PULLDATA;
    // Bypass caching proxies on POSTs and PUTs and avoid writing responses to
    // these requests to the browser's cache
    *bind_flags |= BINDF_GETNEWESTVERSION | BINDF_PRAGMA_NO_CACHE;

    DCHECK(cache_stream_.get());

    // Initialize the STGMEDIUM.
    memset(&bind_info->stgmedData, 0, sizeof(STGMEDIUM));
    bind_info->grfBindInfoF = 0;
    bind_info->szCustomVerb = NULL;
    bind_info->dwBindVerb = BINDVERB_POST;
    bind_info->stgmedData.tymed = TYMED_ISTREAM;
    bind_info->stgmedData.pstm = cache_stream_.get();
    bind_info->stgmedData.pstm->AddRef();
    return BSCBImpl::GetBindInfo(bind_flags, bind_info);
  }

  STDMETHOD(OnResponse)(DWORD response_code, LPCWSTR response_headers,
                        LPCWSTR request_headers, LPWSTR* additional_headers) {
    DVLOG(1) << __FUNCTION__ << " headers: \n" << response_headers;
    return BSCBImpl::OnResponse(response_code, response_headers,
                                request_headers, additional_headers);
  }

 private:
  std::wstring server_url_;
  std::string mime_type_;
  size_t upload_data_size_;
  ScopedComPtr<IStream> cache_stream_;
  ScopedComPtr<IMoniker> upload_moniker_;
};

MetricsService* MetricsService::GetInstance() {
  if (g_metrics_instance_.Pointer()->Get())
    return g_metrics_instance_.Pointer()->Get();

  g_metrics_instance_.Pointer()->Set(new MetricsService);
  return g_metrics_instance_.Pointer()->Get();
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
  base::AutoLock lock(metrics_service_lock_);

  GetInstance()->SetReporting(false);
  GetInstance()->SetRecording(false);
}

void MetricsService::SetRecording(bool enabled) {
  DCHECK_EQ(thread_, base::PlatformThread::CurrentId());
  if (enabled == recording_active_)
    return;

  if (enabled) {
    StartRecording();
  } else {
    state_ = STOPPED;
  }
  recording_active_ = enabled;
}

// static
const std::string& MetricsService::GetClientID() {
  // TODO(robertshield): Chrome Frame shouldn't generate a new ID on every run
  // as this apparently breaks some assumptions during metric analysis.
  // See http://crbug.com/117188
  if (client_id_.empty()) {
    const int kGUIDSize = 39;

    GUID guid;
    HRESULT guid_result = CoCreateGuid(&guid);
    DCHECK(SUCCEEDED(guid_result));

    string16 guid_string;
    int result = StringFromGUID2(guid,
                                 WriteInto(&guid_string, kGUIDSize), kGUIDSize);
    DCHECK(result == kGUIDSize);
    client_id_ = WideToUTF8(guid_string.substr(1, guid_string.length() - 2));
  }
  return client_id_;
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
  if (log_manager_.current_log())
    return;

  MetricsLogManager::LogType log_type = (state_ == INITIALIZED) ?
      MetricsLogManager::INITIAL_LOG : MetricsLogManager::ONGOING_LOG;
  log_manager_.BeginLoggingWithLog(new MetricsLogBase(GetClientID(),
                                                      session_id_,
                                                      GetVersionString()),
                                   log_type);
  if (state_ == INITIALIZED)
    state_ = ACTIVE;
}

void MetricsService::StopRecording(bool save_log) {
  DCHECK_EQ(thread_, base::PlatformThread::CurrentId());
  if (!log_manager_.current_log())
    return;

  // Put incremental histogram deltas at the end of all log transmissions.
  // Don't bother if we're going to discard current_log.
  if (save_log) {
    CrashMetricsReporter::GetInstance()->RecordCrashMetrics();
    RecordCurrentHistograms();
  }

  if (save_log) {
    log_manager_.FinishCurrentLog();
    log_manager_.StageNextLogForUpload();
  } else {
    log_manager_.DiscardCurrentLog();
  }
}

void MetricsService::MakePendingLog() {
  DCHECK_EQ(thread_, base::PlatformThread::CurrentId());
  if (log_manager_.has_staged_log())
    return;

  if (state_ != ACTIVE) {
    NOTREACHED();
    return;
  }

  StopRecording(true);
  StartRecording();
}

bool MetricsService::TransmissionPermitted() const {
  // If the user forbids uploading that's their business, and we don't upload
  // anything.
  return user_permits_upload_;
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

  MakePendingLog();

  bool ret = true;

  if (log_manager_.has_staged_log()) {
    HRESULT hr = ChromeFrameMetricsDataUploader::UploadDataHelper(
        log_manager_.staged_log_text().xml, kServerUrlXml, kMimeTypeXml);
    DCHECK(SUCCEEDED(hr));
    hr = ChromeFrameMetricsDataUploader::UploadDataHelper(
        log_manager_.staged_log_text().proto, kServerUrlProto, kMimeTypeProto);
    DCHECK(SUCCEEDED(hr));
    log_manager_.DiscardStagedLog();
  } else {
    NOTREACHED();
    ret = false;
  }

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
