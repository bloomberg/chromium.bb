// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/net_internals/net_internals_ui.h"

#include <algorithm>
#include <list>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/platform_file.h"
#include "base/prefs/pref_member.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/threading/worker_pool.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/connection_tester.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/cancelable_task_tracker.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/generated_resources.h"
#include "grit/net_internals_resources.h"
#include "net/base/host_cache.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/disk_cache/disk_cache.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_stream_factory.h"
#include "net/http/transport_security_state.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/system/syslogs_provider.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "chromeos/network/onc/onc_constants.h"
#endif
#if defined(OS_WIN)
#include "chrome/browser/net/service_providers_win.h"
#endif

using base::PassPlatformFile;
using base::PlatformFile;
using base::PlatformFileError;
using content::BrowserThread;
using content::WebContents;
using content::WebUIMessageHandler;

namespace {

// Delay between when an event occurs and when it is passed to the Javascript
// page.  All events that occur during this period are grouped together and
// sent to the page at once, which reduces context switching and CPU usage.
const int kNetLogEventDelayMilliseconds = 100;

// about:net-internals will not even attempt to load a log dump when it
// encounters a new version.  This should be incremented when significant
// changes are made that will invalidate the old loading code.
const int kLogFormatVersion = 1;

// Returns the HostCache for |context|'s primary HostResolver, or NULL if
// there is none.
net::HostCache* GetHostResolverCache(net::URLRequestContext* context) {
  return context->host_resolver()->GetHostCache();
}

std::string HashesToBase64String(const net::HashValueVector& hashes) {
  std::string str;
  for (size_t i = 0; i != hashes.size(); ++i) {
    if (i != 0)
      str += ",";
    str += hashes[i].ToString();
  }
  return str;
}

bool Base64StringToHashes(const std::string& hashes_str,
                          net::HashValueVector* hashes) {
  hashes->clear();
  std::vector<std::string> vector_hash_str;
  base::SplitString(hashes_str, ',', &vector_hash_str);

  for (size_t i = 0; i != vector_hash_str.size(); ++i) {
    std::string hash_str;
    RemoveChars(vector_hash_str[i], " \t\r\n", &hash_str);
    net::HashValue hash;
    // Skip past unrecognized hash algos
    // But return false on malformatted input
    if (hash_str.empty())
      return false;
    if (hash_str.compare(0, 5, "sha1/") != 0 &&
        hash_str.compare(0, 7, "sha256/") != 0) {
      continue;
    }
    if (!hash.FromString(hash_str))
      return false;
    hashes->push_back(hash);
  }
  return true;
}

// Returns a Value representing the state of a pre-existing URLRequest when
// net-internals was opened.
Value* RequestStateToValue(const net::URLRequest* request,
                           net::NetLog::LogLevel log_level) {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString("url", request->original_url().possibly_invalid_spec());

  const std::vector<GURL>& url_chain = request->url_chain();
  if (url_chain.size() > 1) {
    ListValue* list = new ListValue();
    for (std::vector<GURL>::const_iterator url = url_chain.begin();
         url != url_chain.end(); ++url) {
      list->AppendString(url->spec());
    }
    dict->Set("url_chain", list);
  }

  dict->SetInteger("load_flags", request->load_flags());

  net::LoadStateWithParam load_state = request->GetLoadState();
  dict->SetInteger("load_state", load_state.state);
  if (!load_state.param.empty())
    dict->SetString("load_state_param", load_state.param);

  dict->SetString("method", request->method());
  dict->SetBoolean("has_upload", request->has_upload());
  dict->SetBoolean("is_pending", request->is_pending());
  return dict;
}

// Returns true if |request1| was created before |request2|.
bool RequestCreatedBefore(const net::URLRequest* request1,
                          const net::URLRequest* request2) {
  return request1->creation_time() < request2->creation_time();
}

// Returns the disk cache backend for |context| if there is one, or NULL.
disk_cache::Backend* GetDiskCacheBackend(net::URLRequestContext* context) {
  if (!context->http_transaction_factory())
    return NULL;

  net::HttpCache* http_cache = context->http_transaction_factory()->GetCache();
  if (!http_cache)
    return NULL;

  return http_cache->GetCurrentBackend();
}

// Returns the http network session for |context| if there is one.
// Otherwise, returns NULL.
net::HttpNetworkSession* GetHttpNetworkSession(
    net::URLRequestContext* context) {
  if (!context->http_transaction_factory())
    return NULL;

  return context->http_transaction_factory()->GetSession();
}

Value* ExperimentToValue(const ConnectionTester::Experiment& experiment) {
  DictionaryValue* dict = new DictionaryValue();

  if (experiment.url.is_valid())
    dict->SetString("url", experiment.url.spec());

  dict->SetString("proxy_settings_experiment",
                  ConnectionTester::ProxySettingsExperimentDescription(
                      experiment.proxy_settings_experiment));
  dict->SetString("host_resolver_experiment",
                  ConnectionTester::HostResolverExperimentDescription(
                      experiment.host_resolver_experiment));
  return dict;
}

content::WebUIDataSource* CreateNetInternalsHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUINetInternalsHost);

  source->SetDefaultResource(IDR_NET_INTERNALS_INDEX_HTML);
  source->AddResourcePath("help.html", IDR_NET_INTERNALS_HELP_HTML);
  source->AddResourcePath("help.js", IDR_NET_INTERNALS_HELP_JS);
  source->AddResourcePath("index.js", IDR_NET_INTERNALS_INDEX_JS);
  source->SetJsonPath("strings.js");
  return source;
}

#if defined(OS_CHROMEOS)
// Small helper class used to create temporary log file and pass its
// handle and error status to callback.
// Use case:
// DebugLogFileHelper* helper = new DebugLogFileHelper();
// base::WorkerPool::PostTaskAndReply(FROM_HERE,
//     base::Bind(&DebugLogFileHelper::DoWork, base::Unretained(helper), ...),
//     base::Bind(&DebugLogFileHelper::Reply, base::Owned(helper), ...),
//     false);
class DebugLogFileHelper {
 public:
  typedef base::Callback<void(PassPlatformFile pass_platform_file,
                              bool created,
                              PlatformFileError error,
                              const base::FilePath& file_path)>
      DebugLogFileCallback;

  DebugLogFileHelper()
      : file_handle_(base::kInvalidPlatformFileValue),
        created_(false),
        error_(base::PLATFORM_FILE_OK) {
  }

  ~DebugLogFileHelper() {
  }

  void DoWork(const base::FilePath& fileshelf) {
    const base::FilePath::CharType kLogFileName[] =
        FILE_PATH_LITERAL("debug-log.tgz");

    file_path_ = fileshelf.Append(kLogFileName);
    file_path_ = logging::GenerateTimestampedName(file_path_,
                                                  base::Time::Now());

    int flags =
        base::PLATFORM_FILE_CREATE_ALWAYS |
        base::PLATFORM_FILE_WRITE;
    file_handle_ = base::CreatePlatformFile(file_path_, flags,
                                            &created_, &error_);
  }

  void Reply(const DebugLogFileCallback& callback) {
    DCHECK(!callback.is_null());
    callback.Run(PassPlatformFile(&file_handle_), created_, error_, file_path_);
  }

 private:
  PlatformFile file_handle_;
  bool created_;
  PlatformFileError error_;
  base::FilePath file_path_;

  DISALLOW_COPY_AND_ASSIGN(DebugLogFileHelper);
};

// Following functions are used for getting debug logs. Logs are
// fetched from /var/log/* and put on the fileshelf.

// Called once StoreDebugLogs is complete. Takes two parameters:
// - log_path: where the log file was saved in the case of success;
// - succeeded: was the log file saved successfully.
typedef base::Callback<void(const base::FilePath& log_path,
                            bool succeded)> StoreDebugLogsCallback;

// Closes file handle, so, should be called on the WorkerPool thread.
void CloseDebugLogFile(PassPlatformFile pass_platform_file) {
  base::ClosePlatformFile(pass_platform_file.ReleaseValue());
}

// Closes file handle and deletes debug log file, so, should be called
// on the WorkerPool thread.
void CloseAndDeleteDebugLogFile(PassPlatformFile pass_platform_file,
                                const base::FilePath& file_path) {
  CloseDebugLogFile(pass_platform_file);
  file_util::Delete(file_path, false);
}

// Called upon completion of |WriteDebugLogToFile|. Closes file
// descriptor, deletes log file in the case of failure and calls
// |callback|.
void WriteDebugLogToFileCompleted(const StoreDebugLogsCallback& callback,
                                  PassPlatformFile pass_platform_file,
                                  const base::FilePath& file_path,
                                  bool succeeded) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!succeeded) {
    bool posted = base::WorkerPool::PostTaskAndReply(FROM_HERE,
        base::Bind(&CloseAndDeleteDebugLogFile, pass_platform_file, file_path),
        base::Bind(callback, file_path, false), false);
    DCHECK(posted);
    return;
  }
  bool posted = base::WorkerPool::PostTaskAndReply(FROM_HERE,
      base::Bind(&CloseDebugLogFile, pass_platform_file),
      base::Bind(callback, file_path, true), false);
  DCHECK(posted);
}

// Stores into |file_path| debug logs in the .tgz format. Calls
// |callback| upon completion.
void WriteDebugLogToFile(const StoreDebugLogsCallback& callback,
                         PassPlatformFile pass_platform_file,
                         bool created,
                         PlatformFileError error,
                         const base::FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!created) {
    LOG(ERROR) <<
        "Can't create debug log file: " << file_path.AsUTF8Unsafe() << ", " <<
        "error: " << error;
    bool posted = base::WorkerPool::PostTaskAndReply(FROM_HERE,
        base::Bind(&CloseDebugLogFile, pass_platform_file),
        base::Bind(callback, file_path, false), false);
    DCHECK(posted);
    return;
  }
  PlatformFile platform_file = pass_platform_file.ReleaseValue();
  chromeos::DBusThreadManager::Get()->GetDebugDaemonClient()->GetDebugLogs(
      platform_file,
      base::Bind(&WriteDebugLogToFileCompleted,
          callback, PassPlatformFile(&platform_file), file_path));
}

// Stores debug logs in the .tgz archive on the fileshelf. The file is
// created on the worker pool, then writing to it is triggered from
// the UI thread, and finally it is closed (on success) or deleted (on
// failure) on the worker pool, prior to calling |callback|.
void StoreDebugLogs(const StoreDebugLogsCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  const base::FilePath fileshelf = download_util::GetDefaultDownloadDirectory();
  DebugLogFileHelper* helper = new DebugLogFileHelper();
  bool posted = base::WorkerPool::PostTaskAndReply(FROM_HERE,
      base::Bind(&DebugLogFileHelper::DoWork,
          base::Unretained(helper), fileshelf),
      base::Bind(&DebugLogFileHelper::Reply, base::Owned(helper),
          base::Bind(&WriteDebugLogToFile, callback)), false);
  DCHECK(posted);
}
#endif  // defined(OS_CHROMEOS)

// This class receives javascript messages from the renderer.
// Note that the WebUI infrastructure runs on the UI thread, therefore all of
// this class's methods are expected to run on the UI thread.
//
// Since the network code we want to run lives on the IO thread, we proxy
// almost everything over to NetInternalsMessageHandler::IOThreadImpl, which
// runs on the IO thread.
//
// TODO(eroman): Can we start on the IO thread to begin with?
class NetInternalsMessageHandler
    : public WebUIMessageHandler,
      public base::SupportsWeakPtr<NetInternalsMessageHandler> {
 public:
  NetInternalsMessageHandler();
  virtual ~NetInternalsMessageHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Calls g_browser.receive in the renderer, passing in |command| and |arg|.
  // Takes ownership of |arg|.  If the renderer is displaying a log file, the
  // message will be ignored.
  void SendJavascriptCommand(const std::string& command, Value* arg);

  // Javascript message handlers.
  void OnRendererReady(const ListValue* list);
  void OnClearBrowserCache(const ListValue* list);
  void OnGetPrerenderInfo(const ListValue* list);
  void OnGetHistoricNetworkStats(const ListValue* list);
#if defined(OS_CHROMEOS)
  void OnRefreshSystemLogs(const ListValue* list);
  void OnGetSystemLog(const ListValue* list);
  void OnImportONCFile(const ListValue* list);
  void OnStoreDebugLogs(const ListValue* list);
  void OnStoreDebugLogsCompleted(const base::FilePath& log_path,
                                 bool succeeded);
  void OnSetNetworkDebugMode(const ListValue* list);
  void OnSetNetworkDebugModeCompleted(const std::string& subsystem,
                                      bool succeeded);
#endif

 private:
  class IOThreadImpl;

#if defined(OS_CHROMEOS)
  // Class that is used for getting network related ChromeOS logs.
  // Logs are fetched from ChromeOS libcros on user request, and only when we
  // don't yet have a copy of logs. If a copy is present, we send back data from
  // it, else we save request and answer to it when we get logs from libcros.
  // If needed, we also send request for system logs to libcros.
  // Logs refresh has to be done explicitly, by deleting old logs and then
  // loading them again.
  class SystemLogsGetter {
   public:
    SystemLogsGetter(NetInternalsMessageHandler* handler,
                     chromeos::system::SyslogsProvider* syslogs_provider);
    ~SystemLogsGetter();

    // Deletes logs copy we currently have, and resets logs_requested and
    // logs_received flags.
    void DeleteSystemLogs();
    // Starts log fetching. If logs copy is present, requested logs are sent
    // back.
    // If syslogs load request hasn't been sent to libcros yet, we do that now,
    // and postpone sending response.
    // Request data is specified by args:
    //   $1 : key of the log we are interested in.
    //   $2 : string used to identify request.
    void RequestSystemLog(const ListValue* args);
    // Requests logs from libcros, but only if we don't have a copy.
    void LoadSystemLogs();
    // Processes callback from libcros containing system logs. Postponed
    // request responses are sent.
    void OnSystemLogsLoaded(chromeos::system::LogDictionaryType* sys_info,
                            std::string* ignored_content);

   private:
    // Struct we save postponed log request in.
    struct SystemLogRequest {
      std::string log_key;
      std::string cell_id;
    };

    // Processes request.
    void SendLogs(const SystemLogRequest& request);

    NetInternalsMessageHandler* handler_;
    chromeos::system::SyslogsProvider* syslogs_provider_;
    // List of postponed requests.
    std::list<SystemLogRequest> requests_;
    scoped_ptr<chromeos::system::LogDictionaryType> logs_;
    bool logs_received_;
    bool logs_requested_;
    CancelableTaskTracker tracker_;
    // Libcros request task ID.
    CancelableTaskTracker::TaskId syslogs_task_id_;
  };
#endif  // defined(OS_CHROMEOS)

  // This is the "real" message handler, which lives on the IO thread.
  scoped_refptr<IOThreadImpl> proxy_;

  base::WeakPtr<prerender::PrerenderManager> prerender_manager_;

#if defined(OS_CHROMEOS)
  // Class that handles getting and filtering system logs.
  scoped_ptr<SystemLogsGetter> syslogs_getter_;
#endif

  DISALLOW_COPY_AND_ASSIGN(NetInternalsMessageHandler);
};

// This class is the "real" message handler. It is allocated and destroyed on
// the UI thread.  With the exception of OnAddEntry, OnWebUIDeleted, and
// SendJavascriptCommand, its methods are all expected to be called from the IO
// thread.  OnAddEntry and SendJavascriptCommand can be called from any thread,
// and OnWebUIDeleted can only be called from the UI thread.
class NetInternalsMessageHandler::IOThreadImpl
    : public base::RefCountedThreadSafe<
          NetInternalsMessageHandler::IOThreadImpl,
          BrowserThread::DeleteOnUIThread>,
      public net::NetLog::ThreadSafeObserver,
      public ConnectionTester::Delegate {
 public:
  // Type for methods that can be used as MessageHandler callbacks.
  typedef void (IOThreadImpl::*MessageHandler)(const ListValue*);

  // Creates a proxy for |handler| that will live on the IO thread.
  // |handler| is a weak pointer, since it is possible for the
  // WebUIMessageHandler to be deleted on the UI thread while we were executing
  // on the IO thread. |io_thread| is the global IOThread (it is passed in as
  // an argument since we need to grab it from the UI thread).
  IOThreadImpl(
      const base::WeakPtr<NetInternalsMessageHandler>& handler,
      IOThread* io_thread,
      net::URLRequestContextGetter* main_context_getter);

  // Called on UI thread just after creation, to add a ContextGetter to
  // |context_getters_|.
  void AddRequestContextGetter(net::URLRequestContextGetter* context_getter);

  // Helper method to enable a callback that will be executed on the IO thread.
  static void CallbackHelper(MessageHandler method,
                             scoped_refptr<IOThreadImpl> io_thread,
                             const ListValue* list);

  // Called once the WebUI has been deleted (i.e. renderer went away), on the
  // IO thread.
  void Detach();

  // Called when the WebUI is deleted.  Prevents calling Javascript functions
  // afterwards.  Called on UI thread.
  void OnWebUIDeleted();

  //--------------------------------
  // Javascript message handlers:
  //--------------------------------

  void OnRendererReady(const ListValue* list);

  void OnGetProxySettings(const ListValue* list);
  void OnReloadProxySettings(const ListValue* list);
  void OnGetBadProxies(const ListValue* list);
  void OnClearBadProxies(const ListValue* list);
  void OnGetHostResolverInfo(const ListValue* list);
  void OnRunIPv6Probe(const ListValue* list);
  void OnClearHostResolverCache(const ListValue* list);
  void OnEnableIPv6(const ListValue* list);
  void OnStartConnectionTests(const ListValue* list);
  void OnHSTSQuery(const ListValue* list);
  void OnHSTSAdd(const ListValue* list);
  void OnHSTSDelete(const ListValue* list);
  void OnGetHttpCacheInfo(const ListValue* list);
  void OnGetSocketPoolInfo(const ListValue* list);
  void OnGetSessionNetworkStats(const ListValue* list);
  void OnCloseIdleSockets(const ListValue* list);
  void OnFlushSocketPools(const ListValue* list);
  void OnGetSpdySessionInfo(const ListValue* list);
  void OnGetSpdyStatus(const ListValue* list);
  void OnGetSpdyAlternateProtocolMappings(const ListValue* list);
  void OnGetQuicInfo(const ListValue* list);
#if defined(OS_WIN)
  void OnGetServiceProviders(const ListValue* list);
#endif
  void OnGetHttpPipeliningStatus(const ListValue* list);
  void OnSetLogLevel(const ListValue* list);

  // ChromeNetLog::ThreadSafeObserver implementation:
  virtual void OnAddEntry(const net::NetLog::Entry& entry) OVERRIDE;

  // ConnectionTester::Delegate implementation:
  virtual void OnStartConnectionTestSuite() OVERRIDE;
  virtual void OnStartConnectionTestExperiment(
      const ConnectionTester::Experiment& experiment) OVERRIDE;
  virtual void OnCompletedConnectionTestExperiment(
      const ConnectionTester::Experiment& experiment,
      int result) OVERRIDE;
  virtual void OnCompletedConnectionTestSuite() OVERRIDE;

  // Helper that calls g_browser.receive in the renderer, passing in |command|
  // and |arg|.  Takes ownership of |arg|.  If the renderer is displaying a log
  // file, the message will be ignored.  Note that this can be called from any
  // thread.
  void SendJavascriptCommand(const std::string& command, Value* arg);

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class base::DeleteHelper<IOThreadImpl>;

  typedef std::list<scoped_refptr<net::URLRequestContextGetter> >
      ContextGetterList;

  virtual ~IOThreadImpl();

  // Adds |entry| to the queue of pending log entries to be sent to the page via
  // Javascript.  Must be called on the IO Thread.  Also creates a delayed task
  // that will call PostPendingEntries, if there isn't one already.
  void AddEntryToQueue(Value* entry);

  // Sends all pending entries to the page via Javascript, and clears the list
  // of pending entries.  Sending multiple entries at once results in a
  // significant reduction of CPU usage when a lot of events are happening.
  // Must be called on the IO Thread.
  void PostPendingEntries();

  // Adds entries with the states of ongoing URL requests.
  void PrePopulateEventList();

  net::URLRequestContext* GetMainContext() {
    return main_context_getter_->GetURLRequestContext();
  }

  // Pointer to the UI-thread message handler. Only access this from
  // the UI thread.
  base::WeakPtr<NetInternalsMessageHandler> handler_;

  // The global IOThread, which contains the global NetLog to observer.
  IOThread* io_thread_;

  // The main URLRequestContextGetter for the tab's profile.
  scoped_refptr<net::URLRequestContextGetter> main_context_getter_;

  // Helper that runs the suite of connection tests.
  scoped_ptr<ConnectionTester> connection_tester_;

  // True if the Web UI has been deleted.  This is used to prevent calling
  // Javascript functions after the Web UI is destroyed.  On refresh, the
  // messages can end up being sent to the refreshed page, causing duplicate
  // or partial entries.
  //
  // This is only read and written to on the UI thread.
  bool was_webui_deleted_;

  // Log entries that have yet to be passed along to Javascript page.  Non-NULL
  // when and only when there is a pending delayed task to call
  // PostPendingEntries.  Read and written to exclusively on the IO Thread.
  scoped_ptr<ListValue> pending_entries_;

  // Used for getting current status of URLRequests when net-internals is
  // opened.  |main_context_getter_| is automatically added on construction.
  // Duplicates are allowed.
  ContextGetterList context_getters_;

  DISALLOW_COPY_AND_ASSIGN(IOThreadImpl);
};

////////////////////////////////////////////////////////////////////////////////
//
// NetInternalsMessageHandler
//
////////////////////////////////////////////////////////////////////////////////

NetInternalsMessageHandler::NetInternalsMessageHandler() {}

NetInternalsMessageHandler::~NetInternalsMessageHandler() {
  if (proxy_) {
    proxy_.get()->OnWebUIDeleted();
    // Notify the handler on the IO thread that the renderer is gone.
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(&IOThreadImpl::Detach, proxy_.get()));
  }
}

void NetInternalsMessageHandler::RegisterMessages() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  Profile* profile = Profile::FromWebUI(web_ui());

  proxy_ = new IOThreadImpl(this->AsWeakPtr(), g_browser_process->io_thread(),
                            profile->GetRequestContext());
  proxy_->AddRequestContextGetter(profile->GetMediaRequestContext());
  proxy_->AddRequestContextGetter(profile->GetRequestContextForExtensions());
#if defined(OS_CHROMEOS)
  syslogs_getter_.reset(new SystemLogsGetter(this,
      chromeos::system::SyslogsProvider::GetInstance()));
#endif

  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForProfile(profile);
  if (prerender_manager) {
    prerender_manager_ = prerender_manager->AsWeakPtr();
  } else {
    prerender_manager_ = base::WeakPtr<prerender::PrerenderManager>();
  }

  web_ui()->RegisterMessageCallback(
      "notifyReady",
      base::Bind(&NetInternalsMessageHandler::OnRendererReady,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getProxySettings",
      base::Bind(&IOThreadImpl::CallbackHelper,
                 &IOThreadImpl::OnGetProxySettings, proxy_));
  web_ui()->RegisterMessageCallback(
      "reloadProxySettings",
      base::Bind(&IOThreadImpl::CallbackHelper,
                 &IOThreadImpl::OnReloadProxySettings, proxy_));
  web_ui()->RegisterMessageCallback(
      "getBadProxies",
      base::Bind(&IOThreadImpl::CallbackHelper,
                 &IOThreadImpl::OnGetBadProxies, proxy_));
  web_ui()->RegisterMessageCallback(
      "clearBadProxies",
      base::Bind(&IOThreadImpl::CallbackHelper,
                 &IOThreadImpl::OnClearBadProxies, proxy_));
  web_ui()->RegisterMessageCallback(
      "getHostResolverInfo",
      base::Bind(&IOThreadImpl::CallbackHelper,
                 &IOThreadImpl::OnGetHostResolverInfo, proxy_));
  web_ui()->RegisterMessageCallback(
      "onRunIPv6Probe",
      base::Bind(&IOThreadImpl::CallbackHelper,
                 &IOThreadImpl::OnRunIPv6Probe, proxy_));
  web_ui()->RegisterMessageCallback(
      "clearHostResolverCache",
      base::Bind(&IOThreadImpl::CallbackHelper,
                 &IOThreadImpl::OnClearHostResolverCache, proxy_));
  web_ui()->RegisterMessageCallback(
      "enableIPv6",
      base::Bind(&IOThreadImpl::CallbackHelper,
                 &IOThreadImpl::OnEnableIPv6, proxy_));
  web_ui()->RegisterMessageCallback(
      "startConnectionTests",
      base::Bind(&IOThreadImpl::CallbackHelper,
                 &IOThreadImpl::OnStartConnectionTests, proxy_));
  web_ui()->RegisterMessageCallback(
      "hstsQuery",
      base::Bind(&IOThreadImpl::CallbackHelper,
                 &IOThreadImpl::OnHSTSQuery, proxy_));
  web_ui()->RegisterMessageCallback(
      "hstsAdd",
      base::Bind(&IOThreadImpl::CallbackHelper,
                 &IOThreadImpl::OnHSTSAdd, proxy_));
  web_ui()->RegisterMessageCallback(
      "hstsDelete",
      base::Bind(&IOThreadImpl::CallbackHelper,
                 &IOThreadImpl::OnHSTSDelete, proxy_));
  web_ui()->RegisterMessageCallback(
      "getHttpCacheInfo",
      base::Bind(&IOThreadImpl::CallbackHelper,
                 &IOThreadImpl::OnGetHttpCacheInfo, proxy_));
  web_ui()->RegisterMessageCallback(
      "getSocketPoolInfo",
      base::Bind(&IOThreadImpl::CallbackHelper,
                 &IOThreadImpl::OnGetSocketPoolInfo, proxy_));
  web_ui()->RegisterMessageCallback(
      "getSessionNetworkStats",
      base::Bind(&IOThreadImpl::CallbackHelper,
                 &IOThreadImpl::OnGetSessionNetworkStats, proxy_));
  web_ui()->RegisterMessageCallback(
      "closeIdleSockets",
      base::Bind(&IOThreadImpl::CallbackHelper,
                 &IOThreadImpl::OnCloseIdleSockets, proxy_));
  web_ui()->RegisterMessageCallback(
      "flushSocketPools",
      base::Bind(&IOThreadImpl::CallbackHelper,
                 &IOThreadImpl::OnFlushSocketPools, proxy_));
  web_ui()->RegisterMessageCallback(
      "getSpdySessionInfo",
      base::Bind(&IOThreadImpl::CallbackHelper,
                 &IOThreadImpl::OnGetSpdySessionInfo, proxy_));
  web_ui()->RegisterMessageCallback(
      "getSpdyStatus",
      base::Bind(&IOThreadImpl::CallbackHelper,
                 &IOThreadImpl::OnGetSpdyStatus, proxy_));
  web_ui()->RegisterMessageCallback(
      "getSpdyAlternateProtocolMappings",
      base::Bind(&IOThreadImpl::CallbackHelper,
                 &IOThreadImpl::OnGetSpdyAlternateProtocolMappings, proxy_));
  web_ui()->RegisterMessageCallback(
      "getQuicInfo",
      base::Bind(&IOThreadImpl::CallbackHelper,
                 &IOThreadImpl::OnGetQuicInfo, proxy_));
#if defined(OS_WIN)
  web_ui()->RegisterMessageCallback(
      "getServiceProviders",
      base::Bind(&IOThreadImpl::CallbackHelper,
                 &IOThreadImpl::OnGetServiceProviders, proxy_));
#endif

  web_ui()->RegisterMessageCallback(
      "getHttpPipeliningStatus",
      base::Bind(&IOThreadImpl::CallbackHelper,
                 &IOThreadImpl::OnGetHttpPipeliningStatus, proxy_));
  web_ui()->RegisterMessageCallback(
      "setLogLevel",
      base::Bind(&IOThreadImpl::CallbackHelper,
                 &IOThreadImpl::OnSetLogLevel, proxy_));
  web_ui()->RegisterMessageCallback(
      "clearBrowserCache",
      base::Bind(&NetInternalsMessageHandler::OnClearBrowserCache,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPrerenderInfo",
      base::Bind(&NetInternalsMessageHandler::OnGetPrerenderInfo,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getHistoricNetworkStats",
      base::Bind(&NetInternalsMessageHandler::OnGetHistoricNetworkStats,
                 base::Unretained(this)));
#if defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      "refreshSystemLogs",
      base::Bind(&NetInternalsMessageHandler::OnRefreshSystemLogs,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSystemLog",
      base::Bind(&NetInternalsMessageHandler::OnGetSystemLog,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "importONCFile",
      base::Bind(&NetInternalsMessageHandler::OnImportONCFile,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "storeDebugLogs",
      base::Bind(&NetInternalsMessageHandler::OnStoreDebugLogs,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setNetworkDebugMode",
      base::Bind(&NetInternalsMessageHandler::OnSetNetworkDebugMode,
                 base::Unretained(this)));
#endif
}

void NetInternalsMessageHandler::SendJavascriptCommand(
    const std::string& command,
    Value* arg) {
  scoped_ptr<Value> command_value(Value::CreateStringValue(command));
  scoped_ptr<Value> value(arg);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (value.get()) {
    web_ui()->CallJavascriptFunction("g_browser.receive",
                                     *command_value.get(),
                                     *value.get());
  } else {
    web_ui()->CallJavascriptFunction("g_browser.receive",
                                     *command_value.get());
  }
}

void NetInternalsMessageHandler::OnRendererReady(const ListValue* list) {
  IOThreadImpl::CallbackHelper(&IOThreadImpl::OnRendererReady, proxy_, list);
}

void NetInternalsMessageHandler::OnClearBrowserCache(const ListValue* list) {
  BrowsingDataRemover* remover = BrowsingDataRemover::CreateForUnboundedRange(
      Profile::FromWebUI(web_ui()));
  remover->Remove(BrowsingDataRemover::REMOVE_CACHE,
                  BrowsingDataHelper::UNPROTECTED_WEB);
  // BrowsingDataRemover deletes itself.
}

void NetInternalsMessageHandler::OnGetPrerenderInfo(const ListValue* list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DictionaryValue* value = NULL;
  prerender::PrerenderManager* prerender_manager = prerender_manager_.get();
  if (!prerender_manager) {
    value = new DictionaryValue();
    value->SetBoolean("enabled", false);
    value->SetBoolean("omnibox_enabled", false);
  } else {
    value = prerender_manager->GetAsValue();
  }
  SendJavascriptCommand("receivedPrerenderInfo", value);
}

void NetInternalsMessageHandler::OnGetHistoricNetworkStats(
    const ListValue* list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Value* historic_network_info =
      ChromeNetworkDelegate::HistoricNetworkStatsInfoToValue();
  SendJavascriptCommand("receivedHistoricNetworkStats", historic_network_info);
}

#if defined(OS_CHROMEOS)
////////////////////////////////////////////////////////////////////////////////
//
// NetInternalsMessageHandler::SystemLogsGetter
//
////////////////////////////////////////////////////////////////////////////////

NetInternalsMessageHandler::SystemLogsGetter::SystemLogsGetter(
    NetInternalsMessageHandler* handler,
    chromeos::system::SyslogsProvider* syslogs_provider)
    : handler_(handler),
      syslogs_provider_(syslogs_provider),
      logs_(NULL),
      logs_received_(false),
      logs_requested_(false) {
  if (!syslogs_provider_)
    LOG(ERROR) << "System access library not loaded";
}

NetInternalsMessageHandler::SystemLogsGetter::~SystemLogsGetter() {
  DeleteSystemLogs();
}

void NetInternalsMessageHandler::SystemLogsGetter::DeleteSystemLogs() {
  if (syslogs_provider_ && logs_requested_ && !logs_received_) {
    tracker_.TryCancel(syslogs_task_id_);
  }
  logs_requested_ = false;
  logs_received_ = false;
  logs_.reset();
}

void NetInternalsMessageHandler::SystemLogsGetter::RequestSystemLog(
    const ListValue* args) {
  if (!logs_requested_) {
    DCHECK(!logs_received_);
    LoadSystemLogs();
  }
  SystemLogRequest log_request;
  args->GetString(0, &log_request.log_key);
  args->GetString(1, &log_request.cell_id);

  if (logs_received_) {
    SendLogs(log_request);
  } else {
    requests_.push_back(log_request);
  }
}

void NetInternalsMessageHandler::SystemLogsGetter::LoadSystemLogs() {
  if (logs_requested_ || !syslogs_provider_)
    return;
  logs_requested_ = true;
  syslogs_task_id_ = syslogs_provider_->RequestSyslogs(
      false,  // compress logs.
      chromeos::system::SyslogsProvider::SYSLOGS_NETWORK,
      base::Bind(
          &NetInternalsMessageHandler::SystemLogsGetter::OnSystemLogsLoaded,
          base::Unretained(this)),
      &tracker_);
}

void NetInternalsMessageHandler::SystemLogsGetter::OnSystemLogsLoaded(
    chromeos::system::LogDictionaryType* sys_info,
    std::string* ignored_content) {
  DCHECK(!ignored_content);
  logs_.reset(sys_info);
  logs_received_ = true;
  for (std::list<SystemLogRequest>::iterator request_it = requests_.begin();
       request_it != requests_.end();
       ++request_it) {
    SendLogs(*request_it);
  }
  requests_.clear();
}

void NetInternalsMessageHandler::SystemLogsGetter::SendLogs(
    const SystemLogRequest& request) {
  DictionaryValue* result = new DictionaryValue();
  chromeos::system::LogDictionaryType::iterator log_it =
      logs_->find(request.log_key);
  if (log_it != logs_->end()) {
    if (!log_it->second.empty()) {
      result->SetString("log", log_it->second);
    } else {
      result->SetString("log", "<no relevant lines found>");
    }
  } else {
    result->SetString("log", "<invalid log name>");
  }
  result->SetString("cellId", request.cell_id);

  handler_->SendJavascriptCommand("getSystemLogCallback", result);
}
#endif  // defined(OS_CHROMEOS)

////////////////////////////////////////////////////////////////////////////////
//
// NetInternalsMessageHandler::IOThreadImpl
//
////////////////////////////////////////////////////////////////////////////////

NetInternalsMessageHandler::IOThreadImpl::IOThreadImpl(
    const base::WeakPtr<NetInternalsMessageHandler>& handler,
    IOThread* io_thread,
    net::URLRequestContextGetter* main_context_getter)
    : handler_(handler),
      io_thread_(io_thread),
      main_context_getter_(main_context_getter),
      was_webui_deleted_(false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  AddRequestContextGetter(main_context_getter);
}

NetInternalsMessageHandler::IOThreadImpl::~IOThreadImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void NetInternalsMessageHandler::IOThreadImpl::AddRequestContextGetter(
    net::URLRequestContextGetter* context_getter) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  context_getters_.push_back(context_getter);
}

void NetInternalsMessageHandler::IOThreadImpl::CallbackHelper(
    MessageHandler method,
    scoped_refptr<IOThreadImpl> io_thread,
    const ListValue* list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // We need to make a copy of the value in order to pass it over to the IO
  // thread. |list_copy| will be deleted when the task is destroyed. The called
  // |method| cannot take ownership of |list_copy|.
  ListValue* list_copy = (list && list->GetSize()) ? list->DeepCopy() : NULL;

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(method, io_thread, base::Owned(list_copy)));
}

void NetInternalsMessageHandler::IOThreadImpl::Detach() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Unregister with network stack to observe events.
  if (net_log())
    net_log()->RemoveThreadSafeObserver(this);

  // Cancel any in-progress connection tests.
  connection_tester_.reset();
}

void NetInternalsMessageHandler::IOThreadImpl::OnWebUIDeleted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  was_webui_deleted_ = true;
}

void NetInternalsMessageHandler::IOThreadImpl::OnRendererReady(
    const ListValue* list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // If we have any pending entries, go ahead and get rid of them, so they won't
  // appear before the REQUEST_ALIVE events we add for currently active
  // URLRequests.
  PostPendingEntries();

  SendJavascriptCommand("receivedConstants", NetInternalsUI::GetConstants());

  // Add entries for ongoing URL requests.
  PrePopulateEventList();

  if (!net_log()) {
    // Register with network stack to observe events.
    io_thread_->net_log()->AddThreadSafeObserver(this,
        net::NetLog::LOG_ALL_BUT_BYTES);
  }
}

void NetInternalsMessageHandler::IOThreadImpl::OnGetProxySettings(
    const ListValue* list) {
  DCHECK(!list);
  net::ProxyService* proxy_service = GetMainContext()->proxy_service();

  DictionaryValue* dict = new DictionaryValue();
  if (proxy_service->fetched_config().is_valid())
    dict->Set("original", proxy_service->fetched_config().ToValue());
  if (proxy_service->config().is_valid())
    dict->Set("effective", proxy_service->config().ToValue());

  SendJavascriptCommand("receivedProxySettings", dict);
}

void NetInternalsMessageHandler::IOThreadImpl::OnReloadProxySettings(
    const ListValue* list) {
  DCHECK(!list);
  GetMainContext()->proxy_service()->ForceReloadProxyConfig();

  // Cause the renderer to be notified of the new values.
  OnGetProxySettings(NULL);
}

void NetInternalsMessageHandler::IOThreadImpl::OnGetBadProxies(
    const ListValue* list) {
  DCHECK(!list);

  const net::ProxyRetryInfoMap& bad_proxies_map =
      GetMainContext()->proxy_service()->proxy_retry_info();

  ListValue* dict_list = new ListValue();

  for (net::ProxyRetryInfoMap::const_iterator it = bad_proxies_map.begin();
       it != bad_proxies_map.end(); ++it) {
    const std::string& proxy_uri = it->first;
    const net::ProxyRetryInfo& retry_info = it->second;

    DictionaryValue* dict = new DictionaryValue();
    dict->SetString("proxy_uri", proxy_uri);
    dict->SetString("bad_until",
                    net::NetLog::TickCountToString(retry_info.bad_until));

    dict_list->Append(dict);
  }

  SendJavascriptCommand("receivedBadProxies", dict_list);
}

void NetInternalsMessageHandler::IOThreadImpl::OnClearBadProxies(
    const ListValue* list) {
  DCHECK(!list);
  GetMainContext()->proxy_service()->ClearBadProxiesCache();

  // Cause the renderer to be notified of the new values.
  OnGetBadProxies(NULL);
}

void NetInternalsMessageHandler::IOThreadImpl::OnGetHostResolverInfo(
    const ListValue* list) {
  DCHECK(!list);
  net::URLRequestContext* context = GetMainContext();
  net::HostCache* cache = GetHostResolverCache(context);

  if (!cache) {
    SendJavascriptCommand("receivedHostResolverInfo", NULL);
    return;
  }

  DictionaryValue* dict = new DictionaryValue();

  base::Value* dns_config = context->host_resolver()->GetDnsConfigAsValue();
  if (dns_config)
    dict->Set("dns_config", dns_config);

  dict->SetInteger(
      "default_address_family",
      static_cast<int>(context->host_resolver()->GetDefaultAddressFamily()));

  DictionaryValue* cache_info_dict = new DictionaryValue();

  cache_info_dict->SetInteger(
      "capacity",
      static_cast<int>(cache->max_entries()));

  ListValue* entry_list = new ListValue();

  net::HostCache::EntryMap::Iterator it(cache->entries());
  for (; it.HasNext(); it.Advance()) {
    const net::HostCache::Key& key = it.key();
    const net::HostCache::Entry& entry = it.value();

    DictionaryValue* entry_dict = new DictionaryValue();

    entry_dict->SetString("hostname", key.hostname);
    entry_dict->SetInteger("address_family",
        static_cast<int>(key.address_family));
    entry_dict->SetString("expiration",
                          net::NetLog::TickCountToString(it.expiration()));

    if (entry.error != net::OK) {
      entry_dict->SetInteger("error", entry.error);
    } else {
      // Append all of the resolved addresses.
      ListValue* address_list = new ListValue();
      for (size_t i = 0; i < entry.addrlist.size(); ++i) {
        address_list->Append(
            Value::CreateStringValue(entry.addrlist[i].ToStringWithoutPort()));
      }
      entry_dict->Set("addresses", address_list);
    }

    entry_list->Append(entry_dict);
  }

  cache_info_dict->Set("entries", entry_list);
  dict->Set("cache", cache_info_dict);

  SendJavascriptCommand("receivedHostResolverInfo", dict);
}

void NetInternalsMessageHandler::IOThreadImpl::OnRunIPv6Probe(
    const ListValue* list) {
  DCHECK(!list);
  net::HostResolver* resolver = GetMainContext()->host_resolver();

  // Have to set the default address family manually before calling
  // ProbeIPv6Support.
  resolver->SetDefaultAddressFamily(net::ADDRESS_FAMILY_UNSPECIFIED);
  resolver->ProbeIPv6Support();
}

void NetInternalsMessageHandler::IOThreadImpl::OnClearHostResolverCache(
    const ListValue* list) {
  DCHECK(!list);
  net::HostCache* cache = GetHostResolverCache(GetMainContext());

  if (cache)
    cache->clear();

  // Cause the renderer to be notified of the new values.
  OnGetHostResolverInfo(NULL);
}

void NetInternalsMessageHandler::IOThreadImpl::OnEnableIPv6(
    const ListValue* list) {
  DCHECK(!list);
  net::HostResolver* host_resolver = GetMainContext()->host_resolver();

  host_resolver->SetDefaultAddressFamily(net::ADDRESS_FAMILY_UNSPECIFIED);

  // Cause the renderer to be notified of the new value.
  OnGetHostResolverInfo(NULL);
}

void NetInternalsMessageHandler::IOThreadImpl::OnStartConnectionTests(
    const ListValue* list) {
  // |value| should be: [<URL to test>].
  string16 url_str;
  CHECK(list->GetString(0, &url_str));

  // Try to fix-up the user provided URL into something valid.
  // For example, turn "www.google.com" into "http://www.google.com".
  GURL url(URLFixerUpper::FixupURL(UTF16ToUTF8(url_str), std::string()));

  connection_tester_.reset(new ConnectionTester(
      this,
      io_thread_->globals()->proxy_script_fetcher_context.get(),
      net_log()));
  connection_tester_->RunAllTests(url);
}

void NetInternalsMessageHandler::IOThreadImpl::OnHSTSQuery(
    const ListValue* list) {
  // |list| should be: [<domain to query>].
  std::string domain;
  CHECK(list->GetString(0, &domain));
  DictionaryValue* result = new DictionaryValue();

  if (!IsStringASCII(domain)) {
    result->SetString("error", "non-ASCII domain name");
  } else {
    net::TransportSecurityState* transport_security_state =
        GetMainContext()->transport_security_state();
    if (!transport_security_state) {
      result->SetString("error", "no TransportSecurityState active");
    } else {
      net::TransportSecurityState::DomainState state;
      const bool found = transport_security_state->GetDomainState(
          domain, true, &state);

      result->SetBoolean("result", found);
      if (found) {
        result->SetInteger("mode", static_cast<int>(state.upgrade_mode));
        result->SetBoolean("subdomains", state.include_subdomains);
        result->SetString("domain", state.domain);
        result->SetDouble("expiry", state.upgrade_expiry.ToDoubleT());
        result->SetDouble("dynamic_spki_hashes_expiry",
                          state.dynamic_spki_hashes_expiry.ToDoubleT());

        result->SetString("static_spki_hashes",
                          HashesToBase64String(state.static_spki_hashes));
        result->SetString("dynamic_spki_hashes",
                          HashesToBase64String(state.dynamic_spki_hashes));
      }
    }
  }

  SendJavascriptCommand("receivedHSTSResult", result);
}

void NetInternalsMessageHandler::IOThreadImpl::OnHSTSAdd(
    const ListValue* list) {
  // |list| should be: [<domain to query>, <include subdomains>, <cert pins>].
  std::string domain;
  CHECK(list->GetString(0, &domain));
  if (!IsStringASCII(domain)) {
    // Silently fail. The user will get a helpful error if they query for the
    // name.
    return;
  }
  bool include_subdomains;
  CHECK(list->GetBoolean(1, &include_subdomains));
  std::string hashes_str;
  CHECK(list->GetString(2, &hashes_str));

  net::TransportSecurityState* transport_security_state =
      GetMainContext()->transport_security_state();
  if (!transport_security_state)
    return;

  base::Time expiry = base::Time::Now() + base::TimeDelta::FromDays(1000);
  net::HashValueVector hashes;
  if (!hashes_str.empty()) {
    if (!Base64StringToHashes(hashes_str, &hashes))
      return;
  }

  transport_security_state->AddHSTS(domain, expiry, include_subdomains);
  transport_security_state->AddHPKP(domain, expiry, include_subdomains,
                                    hashes);
}

void NetInternalsMessageHandler::IOThreadImpl::OnHSTSDelete(
    const ListValue* list) {
  // |list| should be: [<domain to query>].
  std::string domain;
  CHECK(list->GetString(0, &domain));
  if (!IsStringASCII(domain)) {
    // There cannot be a unicode entry in the HSTS set.
    return;
  }
  net::TransportSecurityState* transport_security_state =
      GetMainContext()->transport_security_state();
  if (!transport_security_state)
    return;

  transport_security_state->DeleteDynamicDataForHost(domain);
}

void NetInternalsMessageHandler::IOThreadImpl::OnGetHttpCacheInfo(
    const ListValue* list) {
  DCHECK(!list);
  DictionaryValue* info_dict = new DictionaryValue();
  DictionaryValue* stats_dict = new DictionaryValue();

  disk_cache::Backend* disk_cache = GetDiskCacheBackend(GetMainContext());

  if (disk_cache) {
    // Extract the statistics key/value pairs from the backend.
    std::vector<std::pair<std::string, std::string> > stats;
    disk_cache->GetStats(&stats);
    for (size_t i = 0; i < stats.size(); ++i) {
      stats_dict->Set(stats[i].first,
                      Value::CreateStringValue(stats[i].second));
    }
  }

  info_dict->Set("stats", stats_dict);

  SendJavascriptCommand("receivedHttpCacheInfo", info_dict);
}

void NetInternalsMessageHandler::IOThreadImpl::OnGetSocketPoolInfo(
    const ListValue* list) {
  DCHECK(!list);
  net::HttpNetworkSession* http_network_session =
      GetHttpNetworkSession(GetMainContext());

  Value* socket_pool_info = NULL;
  if (http_network_session)
    socket_pool_info = http_network_session->SocketPoolInfoToValue();

  SendJavascriptCommand("receivedSocketPoolInfo", socket_pool_info);
}

void NetInternalsMessageHandler::IOThreadImpl::OnGetSessionNetworkStats(
    const ListValue* list) {
  DCHECK(!list);
  net::HttpNetworkSession* http_network_session =
      GetHttpNetworkSession(main_context_getter_->GetURLRequestContext());

  Value* network_info = NULL;
  if (http_network_session) {
    ChromeNetworkDelegate* net_delegate =
        static_cast<ChromeNetworkDelegate*>(
            http_network_session->network_delegate());
    if (net_delegate) {
      network_info = net_delegate->SessionNetworkStatsInfoToValue();
    }
  }
  SendJavascriptCommand("receivedSessionNetworkStats", network_info);
}

void NetInternalsMessageHandler::IOThreadImpl::OnFlushSocketPools(
    const ListValue* list) {
  DCHECK(!list);
  net::HttpNetworkSession* http_network_session =
      GetHttpNetworkSession(GetMainContext());

  if (http_network_session)
    http_network_session->CloseAllConnections();
}

void NetInternalsMessageHandler::IOThreadImpl::OnCloseIdleSockets(
    const ListValue* list) {
  DCHECK(!list);
  net::HttpNetworkSession* http_network_session =
      GetHttpNetworkSession(GetMainContext());

  if (http_network_session)
    http_network_session->CloseIdleConnections();
}

void NetInternalsMessageHandler::IOThreadImpl::OnGetSpdySessionInfo(
    const ListValue* list) {
  DCHECK(!list);
  net::HttpNetworkSession* http_network_session =
      GetHttpNetworkSession(GetMainContext());

  Value* spdy_info = http_network_session ?
      http_network_session->SpdySessionPoolInfoToValue() : NULL;
  SendJavascriptCommand("receivedSpdySessionInfo", spdy_info);
}

void NetInternalsMessageHandler::IOThreadImpl::OnGetSpdyStatus(
    const ListValue* list) {
  DCHECK(!list);
  DictionaryValue* status_dict = new DictionaryValue();

  status_dict->Set("spdy_enabled",
                   Value::CreateBooleanValue(
                       net::HttpStreamFactory::spdy_enabled()));
  status_dict->Set("use_alternate_protocols",
                   Value::CreateBooleanValue(
                       net::HttpStreamFactory::use_alternate_protocols()));
  status_dict->Set("force_spdy_over_ssl",
                   Value::CreateBooleanValue(
                       net::HttpStreamFactory::force_spdy_over_ssl()));
  status_dict->Set("force_spdy_always",
                   Value::CreateBooleanValue(
                       net::HttpStreamFactory::force_spdy_always()));

  // The next_protos may not be specified for certain configurations of SPDY.
  Value* next_protos_value;
  if (net::HttpStreamFactory::has_next_protos()) {
    next_protos_value = Value::CreateStringValue(
        JoinString(net::HttpStreamFactory::next_protos(), ','));
  } else {
    next_protos_value = Value::CreateStringValue("");
  }
  status_dict->Set("next_protos", next_protos_value);

  SendJavascriptCommand("receivedSpdyStatus", status_dict);
}

void
NetInternalsMessageHandler::IOThreadImpl::OnGetSpdyAlternateProtocolMappings(
    const ListValue* list) {
  DCHECK(!list);
  ListValue* dict_list = new ListValue();

  const net::HttpServerProperties& http_server_properties =
      *GetMainContext()->http_server_properties();

  const net::AlternateProtocolMap& map =
      http_server_properties.alternate_protocol_map();

  for (net::AlternateProtocolMap::const_iterator it = map.begin();
       it != map.end(); ++it) {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetString("host_port_pair", it->first.ToString());
    dict->SetString("alternate_protocol", it->second.ToString());
    dict_list->Append(dict);
  }

  SendJavascriptCommand("receivedSpdyAlternateProtocolMappings", dict_list);
}

void NetInternalsMessageHandler::IOThreadImpl::OnGetQuicInfo(
    const ListValue* list) {
  DCHECK(!list);
  net::HttpNetworkSession* http_network_session =
      GetHttpNetworkSession(GetMainContext());

  Value* quic_info = http_network_session ?
      http_network_session->QuicInfoToValue() : NULL;
  SendJavascriptCommand("receivedQuicInfo", quic_info);
}

#if defined(OS_WIN)
void NetInternalsMessageHandler::IOThreadImpl::OnGetServiceProviders(
    const ListValue* list) {
  DCHECK(!list);

  DictionaryValue* service_providers = new DictionaryValue();

  WinsockLayeredServiceProviderList layered_providers;
  GetWinsockLayeredServiceProviders(&layered_providers);
  ListValue* layered_provider_list = new ListValue();
  for (size_t i = 0; i < layered_providers.size(); ++i) {
    DictionaryValue* service_dict = new DictionaryValue();
    service_dict->SetString("name", layered_providers[i].name);
    service_dict->SetInteger("version", layered_providers[i].version);
    service_dict->SetInteger("chain_length", layered_providers[i].chain_length);
    service_dict->SetInteger("socket_type", layered_providers[i].socket_type);
    service_dict->SetInteger("socket_protocol",
        layered_providers[i].socket_protocol);
    service_dict->SetString("path", layered_providers[i].path);

    layered_provider_list->Append(service_dict);
  }
  service_providers->Set("service_providers", layered_provider_list);

  WinsockNamespaceProviderList namespace_providers;
  GetWinsockNamespaceProviders(&namespace_providers);
  ListValue* namespace_list = new ListValue;
  for (size_t i = 0; i < namespace_providers.size(); ++i) {
    DictionaryValue* namespace_dict = new DictionaryValue();
    namespace_dict->SetString("name", namespace_providers[i].name);
    namespace_dict->SetBoolean("active", namespace_providers[i].active);
    namespace_dict->SetInteger("version", namespace_providers[i].version);
    namespace_dict->SetInteger("type", namespace_providers[i].type);

    namespace_list->Append(namespace_dict);
  }
  service_providers->Set("namespace_providers", namespace_list);

  SendJavascriptCommand("receivedServiceProviders", service_providers);
}
#endif

#if defined(OS_CHROMEOS)
void NetInternalsMessageHandler::OnRefreshSystemLogs(const ListValue* list) {
  DCHECK(!list);
  DCHECK(syslogs_getter_.get());
  syslogs_getter_->DeleteSystemLogs();
  syslogs_getter_->LoadSystemLogs();
}

void NetInternalsMessageHandler::OnGetSystemLog(const ListValue* list) {
  DCHECK(syslogs_getter_.get());
  syslogs_getter_->RequestSystemLog(list);
}

void NetInternalsMessageHandler::OnImportONCFile(const ListValue* list) {
  std::string onc_blob;
  std::string passcode;
  if (list->GetSize() != 2 ||
      !list->GetString(0, &onc_blob) ||
      !list->GetString(1, &passcode)) {
    NOTREACHED();
  }

  std::string error;
  chromeos::NetworkLibrary* cros_network =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  if (!cros_network->LoadOncNetworks(onc_blob, passcode,
                                     chromeos::onc::ONC_SOURCE_USER_IMPORT,
                                     false)) {  // allow web trust from policy
    error = "Errors occurred during the ONC import.";
    LOG(ERROR) << error;
  }

  // Now that we've added the networks, we need to rescan them so they'll be
  // available from the menu more immediately.
  cros_network->RequestNetworkScan();

  SendJavascriptCommand("receivedONCFileParse",
                        Value::CreateStringValue(error));
}

void NetInternalsMessageHandler::OnStoreDebugLogs(const ListValue* list) {
  DCHECK(list);
  StoreDebugLogs(
      base::Bind(&NetInternalsMessageHandler::OnStoreDebugLogsCompleted,
                 AsWeakPtr()));
}

void NetInternalsMessageHandler::OnStoreDebugLogsCompleted(
    const base::FilePath& log_path, bool succeeded) {
  std::string status;
  if (succeeded)
    status = "Created log file: " + log_path.BaseName().AsUTF8Unsafe();
  else
    status = "Failed to create log file";
  SendJavascriptCommand("receivedStoreDebugLogs",
                        Value::CreateStringValue(status));
}

void NetInternalsMessageHandler::OnSetNetworkDebugMode(const ListValue* list) {
  std::string subsystem;
  if (list->GetSize() != 1 || !list->GetString(0, &subsystem))
    NOTREACHED();
  chromeos::DBusThreadManager::Get()->GetDebugDaemonClient()->
      SetDebugMode(
          subsystem,
          base::Bind(
              &NetInternalsMessageHandler::OnSetNetworkDebugModeCompleted,
              AsWeakPtr(),
              subsystem));
}

void NetInternalsMessageHandler::OnSetNetworkDebugModeCompleted(
    const std::string& subsystem,
    bool succeeded) {
  std::string status;
  if (succeeded)
    status = "Debug mode is changed to " + subsystem;
  else
    status = "Failed to change debug mode to " + subsystem;
  SendJavascriptCommand("receivedSetNetworkDebugMode",
                        Value::CreateStringValue(status));
}
#endif  // defined(OS_CHROMEOS)

void NetInternalsMessageHandler::IOThreadImpl::OnGetHttpPipeliningStatus(
    const ListValue* list) {
  DCHECK(!list);
  DictionaryValue* status_dict = new DictionaryValue();

  net::HttpNetworkSession* http_network_session =
      GetHttpNetworkSession(GetMainContext());
  status_dict->Set("pipelining_enabled", Value::CreateBooleanValue(
      http_network_session->params().http_pipelining_enabled));
  Value* pipelined_connection_info = NULL;
  if (http_network_session) {
    pipelined_connection_info =
        http_network_session->http_stream_factory()->PipelineInfoToValue();
  }
  status_dict->Set("pipelined_connection_info", pipelined_connection_info);

  const net::HttpServerProperties& http_server_properties =
      *GetMainContext()->http_server_properties();

  // TODO(simonjam): This call is slow.
  const net::PipelineCapabilityMap pipeline_capability_map =
      http_server_properties.GetPipelineCapabilityMap();

  ListValue* known_hosts_list = new ListValue();
  net::PipelineCapabilityMap::const_iterator it;
  for (it = pipeline_capability_map.begin();
       it != pipeline_capability_map.end(); ++it) {
    DictionaryValue* host_dict = new DictionaryValue();
    host_dict->SetString("host", it->first.ToString());
    std::string capability;
    switch (it->second) {
      case net::PIPELINE_CAPABLE:
        capability = "capable";
        break;

      case net::PIPELINE_PROBABLY_CAPABLE:
        capability = "probably capable";
        break;

      case net::PIPELINE_INCAPABLE:
        capability = "incapable";
        break;

      case net::PIPELINE_UNKNOWN:
      default:
        capability = "unknown";
        break;
    }
    host_dict->SetString("capability", capability);
    known_hosts_list->Append(host_dict);
  }
  status_dict->Set("pipelined_host_info", known_hosts_list);

  SendJavascriptCommand("receivedHttpPipeliningStatus", status_dict);
}

void NetInternalsMessageHandler::IOThreadImpl::OnSetLogLevel(
    const ListValue* list) {
  int log_level;
  std::string log_level_string;
  if (!list->GetString(0, &log_level_string) ||
      !base::StringToInt(log_level_string, &log_level)) {
    NOTREACHED();
    return;
  }

  DCHECK_GE(log_level, net::NetLog::LOG_ALL);
  DCHECK_LE(log_level, net::NetLog::LOG_BASIC);
  net_log()->SetObserverLogLevel(
      this, static_cast<net::NetLog::LogLevel>(log_level));
}

// Note that unlike other methods of IOThreadImpl, this function
// can be called from ANY THREAD.
void NetInternalsMessageHandler::IOThreadImpl::OnAddEntry(
    const net::NetLog::Entry& entry) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&IOThreadImpl::AddEntryToQueue, this, entry.ToValue()));
}

void NetInternalsMessageHandler::IOThreadImpl::OnStartConnectionTestSuite() {
  SendJavascriptCommand("receivedStartConnectionTestSuite", NULL);
}

void NetInternalsMessageHandler::IOThreadImpl::OnStartConnectionTestExperiment(
    const ConnectionTester::Experiment& experiment) {
  SendJavascriptCommand(
      "receivedStartConnectionTestExperiment",
      ExperimentToValue(experiment));
}

void
NetInternalsMessageHandler::IOThreadImpl::OnCompletedConnectionTestExperiment(
    const ConnectionTester::Experiment& experiment,
    int result) {
  DictionaryValue* dict = new DictionaryValue();

  dict->Set("experiment", ExperimentToValue(experiment));
  dict->SetInteger("result", result);

  SendJavascriptCommand(
      "receivedCompletedConnectionTestExperiment",
      dict);
}

void
NetInternalsMessageHandler::IOThreadImpl::OnCompletedConnectionTestSuite() {
  SendJavascriptCommand(
      "receivedCompletedConnectionTestSuite",
      NULL);
}

// Note that this can be called from ANY THREAD.
void NetInternalsMessageHandler::IOThreadImpl::SendJavascriptCommand(
    const std::string& command,
    Value* arg) {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    if (handler_ && !was_webui_deleted_) {
      // We check |handler_| in case it was deleted on the UI thread earlier
      // while we were running on the IO thread.
      handler_->SendJavascriptCommand(command, arg);
    } else {
      delete arg;
    }
    return;
  }

  if (!BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&IOThreadImpl::SendJavascriptCommand, this, command, arg))) {
    // Failed posting the task, avoid leaking.
    delete arg;
  }
}

void NetInternalsMessageHandler::IOThreadImpl::AddEntryToQueue(Value* entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!pending_entries_.get()) {
    pending_entries_.reset(new ListValue());
    BrowserThread::PostDelayedTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&IOThreadImpl::PostPendingEntries, this),
        base::TimeDelta::FromMilliseconds(kNetLogEventDelayMilliseconds));
  }
  pending_entries_->Append(entry);
}

void NetInternalsMessageHandler::IOThreadImpl::PostPendingEntries() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (pending_entries_.get())
    SendJavascriptCommand("receivedLogEntries", pending_entries_.release());
}

void NetInternalsMessageHandler::IOThreadImpl::PrePopulateEventList() {
  // Use a set to prevent duplicates.
  std::set<net::URLRequestContext*> contexts;
  for (ContextGetterList::const_iterator getter = context_getters_.begin();
       getter != context_getters_.end(); ++getter) {
    contexts.insert((*getter)->GetURLRequestContext());
  }
  contexts.insert(io_thread_->globals()->proxy_script_fetcher_context.get());
  contexts.insert(io_thread_->globals()->system_request_context.get());

  // Put together the list of all requests.
  std::vector<const net::URLRequest*> requests;
  for (std::set<net::URLRequestContext*>::const_iterator context =
           contexts.begin();
       context != contexts.end(); ++context) {
    std::set<const net::URLRequest*>* context_requests =
        (*context)->url_requests();
    for (std::set<const net::URLRequest*>::const_iterator request_it =
             context_requests->begin();
         request_it != context_requests->end(); ++request_it) {
      DCHECK_EQ(io_thread_->net_log(), (*request_it)->net_log().net_log());
      requests.push_back(*request_it);
    }
  }

  // Sort by creation time.
  std::sort(requests.begin(), requests.end(), RequestCreatedBefore);

  // Create fake events.
  for (std::vector<const net::URLRequest*>::const_iterator request_it =
           requests.begin();
       request_it != requests.end(); ++request_it) {
    const net::URLRequest* request = *request_it;
    net::NetLog::ParametersCallback callback =
        base::Bind(&RequestStateToValue, base::Unretained(request));

    // Create and add the entry directly, to avoid sending it to any other
    // NetLog observers.
    net::NetLog::Entry entry(net::NetLog::TYPE_REQUEST_ALIVE,
                             request->net_log().source(),
                             net::NetLog::PHASE_BEGIN,
                             request->creation_time(),
                             &callback,
                             request->net_log().GetLogLevel());

    // Have to add |entry| to the queue synchronously, as there may already
    // be posted tasks queued up to add other events for |request|, which we
    // want |entry| to precede.
    AddEntryToQueue(entry.ToValue());
  }
}

}  // namespace


////////////////////////////////////////////////////////////////////////////////
//
// NetInternalsUI
//
////////////////////////////////////////////////////////////////////////////////

// static
Value* NetInternalsUI::GetConstants() {
  DictionaryValue* constants_dict = new DictionaryValue();

  // Version of the file format.
  constants_dict->SetInteger("logFormatVersion", kLogFormatVersion);

  // Add a dictionary with information on the relationship between event type
  // enums and their symbolic names.
  constants_dict->Set("logEventTypes", net::NetLog::GetEventTypesAsValue());

  // Add a dictionary with the version of the client and its command line
  // arguments.
  {
    DictionaryValue* dict = new DictionaryValue();

    chrome::VersionInfo version_info;

    if (!version_info.is_valid()) {
      DLOG(ERROR) << "Unable to create chrome::VersionInfo";
    } else {
      // We have everything we need to send the right values.
      dict->SetString("name", version_info.Name());
      dict->SetString("version", version_info.Version());
      dict->SetString("cl", version_info.LastChange());
      dict->SetString("version_mod",
                      chrome::VersionInfo::GetVersionStringModifier());
      dict->SetString("official",
                      version_info.IsOfficialBuild() ? "official" :
                                                       "unofficial");
      dict->SetString("os_type", version_info.OSType());
      dict->SetString("command_line",
                      CommandLine::ForCurrentProcess()->GetCommandLineString());
    }

    constants_dict->Set("clientInfo", dict);
  }

  // Add a dictionary with information about the relationship between load flag
  // enums and their symbolic names.
  {
    DictionaryValue* dict = new DictionaryValue();

#define LOAD_FLAG(label, value) \
    dict->SetInteger(# label, static_cast<int>(value));
#include "net/base/load_flags_list.h"
#undef LOAD_FLAG

    constants_dict->Set("loadFlag", dict);
  }

  // Add a dictionary with information about the relationship between load state
  // enums and their symbolic names.
  {
    DictionaryValue* dict = new DictionaryValue();

#define LOAD_STATE(label) \
    dict->SetInteger(# label, net::LOAD_STATE_ ## label);
#include "net/base/load_states_list.h"
#undef LOAD_STATE

    constants_dict->Set("loadState", dict);
  }

  // Add information on the relationship between net error codes and their
  // symbolic names.
  {
    DictionaryValue* dict = new DictionaryValue();

#define NET_ERROR(label, value) \
    dict->SetInteger(# label, static_cast<int>(value));
#include "net/base/net_error_list.h"
#undef NET_ERROR

    constants_dict->Set("netError", dict);
  }

  // Information about the relationship between event phase enums and their
  // symbolic names.
  {
    DictionaryValue* dict = new DictionaryValue();

    dict->SetInteger("PHASE_BEGIN", net::NetLog::PHASE_BEGIN);
    dict->SetInteger("PHASE_END", net::NetLog::PHASE_END);
    dict->SetInteger("PHASE_NONE", net::NetLog::PHASE_NONE);

    constants_dict->Set("logEventPhase", dict);
  }

  // Information about the relationship between source type enums and
  // their symbolic names.
  constants_dict->Set("logSourceType", net::NetLog::GetSourceTypesAsValue());

  // Information about the relationship between LogLevel enums and their
  // symbolic names.
  {
    DictionaryValue* dict = new DictionaryValue();

    dict->SetInteger("LOG_ALL", net::NetLog::LOG_ALL);
    dict->SetInteger("LOG_ALL_BUT_BYTES", net::NetLog::LOG_ALL_BUT_BYTES);
    dict->SetInteger("LOG_BASIC", net::NetLog::LOG_BASIC);

    constants_dict->Set("logLevelType", dict);
  }

  // Information about the relationship between address family enums and
  // their symbolic names.
  {
    DictionaryValue* dict = new DictionaryValue();

    dict->SetInteger("ADDRESS_FAMILY_UNSPECIFIED",
                     net::ADDRESS_FAMILY_UNSPECIFIED);
    dict->SetInteger("ADDRESS_FAMILY_IPV4",
                     net::ADDRESS_FAMILY_IPV4);
    dict->SetInteger("ADDRESS_FAMILY_IPV6",
                     net::ADDRESS_FAMILY_IPV6);

    constants_dict->Set("addressFamily", dict);
  }

  // Information about how the "time ticks" values we have given it relate to
  // actual system times. (We used time ticks throughout since they are stable
  // across system clock changes).
  {
    int64 cur_time_ms = (base::Time::Now() - base::Time()).InMilliseconds();

    int64 cur_time_ticks_ms =
        (base::TimeTicks::Now() - base::TimeTicks()).InMilliseconds();

    // If we add this number to a time tick value, it gives the timestamp.
    int64 tick_to_time_ms = cur_time_ms - cur_time_ticks_ms;

    // Chrome on all platforms stores times using the Windows epoch
    // (Jan 1 1601), but the javascript wants a unix epoch.
    // TODO(eroman): Getting the timestamp relative to the unix epoch should
    //               be part of the time library.
    const int64 kUnixEpochMs = 11644473600000LL;
    int64 tick_to_unix_time_ms = tick_to_time_ms - kUnixEpochMs;

    // Pass it as a string, since it may be too large to fit in an integer.
    constants_dict->SetString("timeTickOffset",
                              base::Int64ToString(tick_to_unix_time_ms));
  }
  return constants_dict;
}

NetInternalsUI::NetInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new NetInternalsMessageHandler());

  // Set up the chrome://net-internals/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateNetInternalsHTMLSource());
}
