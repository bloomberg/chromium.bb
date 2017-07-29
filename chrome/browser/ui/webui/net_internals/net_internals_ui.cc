// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/net_internals/net_internals_ui.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/net_export_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/net_internals_resources.h"
#include "components/net_log/chrome_net_log.h"
#include "components/onc/onc_constants.h"
#include "components/prefs/pref_member.h"
#include "components/url_formatter/url_fixer.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_stream_factory.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_entry.h"
#include "net/log/net_log_util.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/file_manager/filesystem_api_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/system_logs/debug_log_writer.h"
#include "chrome/browser/net/nss_context.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "chromeos/network/onc/onc_certificate_importer_impl.h"
#include "chromeos/network/onc/onc_utils.h"
#endif

using base::Value;
using content::BrowserThread;
using content::WebContents;
using content::WebUIMessageHandler;

namespace {

// Delay between when an event occurs and when it is passed to the Javascript
// page.  All events that occur during this period are grouped together and
// sent to the page at once, which reduces context switching and CPU usage.
const int kNetLogEventDelayMilliseconds = 100;

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
  std::vector<std::string> vector_hash_str = base::SplitString(
      hashes_str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  for (size_t i = 0; i != vector_hash_str.size(); ++i) {
    std::string hash_str;
    base::RemoveChars(vector_hash_str[i], " \t\r\n", &hash_str);
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

// Returns the http network session for |context| if there is one.
// Otherwise, returns NULL.
net::HttpNetworkSession* GetHttpNetworkSession(
    net::URLRequestContext* context) {
  if (!context->http_transaction_factory())
    return nullptr;
  return context->http_transaction_factory()->GetSession();
}

content::WebUIDataSource* CreateNetInternalsHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUINetInternalsHost);

  source->SetDefaultResource(IDR_NET_INTERNALS_INDEX_HTML);
  source->AddResourcePath("index.js", IDR_NET_INTERNALS_INDEX_JS);
  source->SetJsonPath("strings.js");
  source->UseGzip(std::unordered_set<std::string>());
  return source;
}

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
  ~NetInternalsMessageHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Calls g_browser.receive in the renderer, passing in |command| and |arg|.
  // If the renderer is displaying a log file, the message will be ignored.
  void SendJavascriptCommand(const std::string& command,
                             std::unique_ptr<base::Value> arg);

  // Javascript message handlers.
  void OnRendererReady(const base::ListValue* list);
  void OnClearBrowserCache(const base::ListValue* list);
  void OnGetPrerenderInfo(const base::ListValue* list);
  void OnGetHistoricNetworkStats(const base::ListValue* list);
  void OnGetSessionNetworkStats(const base::ListValue* list);
  void OnGetExtensionInfo(const base::ListValue* list);
  void OnGetDataReductionProxyInfo(const base::ListValue* list);
#if defined(OS_CHROMEOS)
  void OnImportONCFile(const base::ListValue* list);
  void OnStoreDebugLogs(const base::ListValue* list);
  void OnStoreDebugLogsCompleted(const base::FilePath& log_path,
                                 bool succeeded);
  void OnSetNetworkDebugMode(const base::ListValue* list);
  void OnSetNetworkDebugModeCompleted(const std::string& subsystem,
                                      bool succeeded);

  // Callback to |GetNSSCertDatabaseForProfile| used to retrieve the database
  // to which user's ONC defined certificates should be imported.
  // It parses and imports |onc_blob|.
  void ImportONCFileToNSSDB(const std::string& onc_blob,
                            const std::string& passcode,
                            net::NSSCertDatabase* nssdb);

  // Called back by the CertificateImporter when a certificate import finished.
  // |previous_error| contains earlier errors during this import.
  void OnCertificatesImported(
      const std::string& previous_error,
      bool success,
      const net::CertificateList& onc_trusted_certificates);
#endif

 private:
  class IOThreadImpl;

  // This is the "real" message handler, which lives on the IO thread.
  scoped_refptr<IOThreadImpl> proxy_;

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
      public net::NetLog::ThreadSafeObserver {
 public:
  // Type for methods that can be used as MessageHandler callbacks.
  typedef void (IOThreadImpl::*MessageHandler)(const base::ListValue*);

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
                             const base::ListValue* list);

  // Called once the WebUI has been deleted (i.e. renderer went away), on the
  // IO thread.
  void Detach();

  // Called when the WebUI is deleted.  Prevents calling Javascript functions
  // afterwards.  Called on UI thread.
  void OnWebUIDeleted();

  //--------------------------------
  // Javascript message handlers:
  //--------------------------------

  void OnRendererReady(const base::ListValue* list);

  void OnGetNetInfo(const base::ListValue* list);
  void OnReloadProxySettings(const base::ListValue* list);
  void OnClearBadProxies(const base::ListValue* list);
  void OnClearHostResolverCache(const base::ListValue* list);
  void OnHSTSQuery(const base::ListValue* list);
  void OnHSTSAdd(const base::ListValue* list);
  void OnHSTSDelete(const base::ListValue* list);
  void OnCloseIdleSockets(const base::ListValue* list);
  void OnFlushSocketPools(const base::ListValue* list);
#if defined(OS_WIN)
  void OnGetServiceProviders(const base::ListValue* list);
#endif
  void OnSetCaptureMode(const base::ListValue* list);

  // NetLog::ThreadSafeObserver implementation:
  void OnAddEntry(const net::NetLogEntry& entry) override;

  // Helper that calls g_browser.receive in the renderer, passing in |command|
  // and |arg|.  If the renderer is displaying a log file, the message will be
  // ignored.  Note that this can be called from any thread.
  void SendJavascriptCommand(const std::string& command,
                             std::unique_ptr<base::Value> arg);

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class base::DeleteHelper<IOThreadImpl>;

  using ContextGetterList =
      std::vector<scoped_refptr<net::URLRequestContextGetter>>;

  ~IOThreadImpl() override;

  // Adds |entry| to the queue of pending log entries to be sent to the page via
  // Javascript.  Must be called on the IO Thread.  Also creates a delayed task
  // that will call PostPendingEntries, if there isn't one already.
  void AddEntryToQueue(std::unique_ptr<base::Value> entry);

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

  // |info_sources| is an or'd together list of the net::NetInfoSources to
  // send information about.  Information is sent to Javascript in the form of
  // a single dictionary with information about all requests sources.
  void SendNetInfo(int info_sources);

  // Pointer to the UI-thread message handler. Only access this from
  // the UI thread.
  base::WeakPtr<NetInternalsMessageHandler> handler_;

  // The global IOThread, which contains the global NetLog to observer.
  IOThread* io_thread_;

  // The main URLRequestContextGetter for the tab's profile.
  scoped_refptr<net::URLRequestContextGetter> main_context_getter_;

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
  std::unique_ptr<base::ListValue> pending_entries_;

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
    proxy_->OnWebUIDeleted();
    // Notify the handler on the IO thread that the renderer is gone.
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::BindOnce(&IOThreadImpl::Detach, proxy_));
  }
}

void NetInternalsMessageHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Profile* profile = Profile::FromWebUI(web_ui());

  proxy_ = new IOThreadImpl(this->AsWeakPtr(), g_browser_process->io_thread(),
                            profile->GetRequestContext());
  proxy_->AddRequestContextGetter(
      content::BrowserContext::GetDefaultStoragePartition(profile)->
          GetMediaURLRequestContext());

  web_ui()->RegisterMessageCallback(
      "notifyReady",
      base::Bind(&NetInternalsMessageHandler::OnRendererReady,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getNetInfo",
      base::Bind(&IOThreadImpl::CallbackHelper,
                 &IOThreadImpl::OnGetNetInfo, proxy_));
  web_ui()->RegisterMessageCallback(
      "reloadProxySettings",
      base::Bind(&IOThreadImpl::CallbackHelper,
                 &IOThreadImpl::OnReloadProxySettings, proxy_));
  web_ui()->RegisterMessageCallback(
      "clearBadProxies",
      base::Bind(&IOThreadImpl::CallbackHelper,
                 &IOThreadImpl::OnClearBadProxies, proxy_));
  web_ui()->RegisterMessageCallback(
      "clearHostResolverCache",
      base::Bind(&IOThreadImpl::CallbackHelper,
                 &IOThreadImpl::OnClearHostResolverCache, proxy_));
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
      "closeIdleSockets",
      base::Bind(&IOThreadImpl::CallbackHelper,
                 &IOThreadImpl::OnCloseIdleSockets, proxy_));
  web_ui()->RegisterMessageCallback(
      "flushSocketPools",
      base::Bind(&IOThreadImpl::CallbackHelper,
                 &IOThreadImpl::OnFlushSocketPools, proxy_));
#if defined(OS_WIN)
  web_ui()->RegisterMessageCallback(
      "getServiceProviders",
      base::Bind(&IOThreadImpl::CallbackHelper,
                 &IOThreadImpl::OnGetServiceProviders, proxy_));
#endif

  web_ui()->RegisterMessageCallback(
      "setCaptureMode", base::Bind(&IOThreadImpl::CallbackHelper,
                                   &IOThreadImpl::OnSetCaptureMode, proxy_));
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
  web_ui()->RegisterMessageCallback(
      "getSessionNetworkStats",
      base::Bind(&NetInternalsMessageHandler::OnGetSessionNetworkStats,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getExtensionInfo",
      base::Bind(&NetInternalsMessageHandler::OnGetExtensionInfo,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getDataReductionProxyInfo",
      base::Bind(&NetInternalsMessageHandler::OnGetDataReductionProxyInfo,
                 base::Unretained(this)));
#if defined(OS_CHROMEOS)
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
    std::unique_ptr<base::Value> arg) {
  std::unique_ptr<base::Value> command_value(new base::Value(command));
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (arg) {
    web_ui()->CallJavascriptFunctionUnsafe("g_browser.receive",
                                           *command_value.get(), *arg.get());
  } else {
    web_ui()->CallJavascriptFunctionUnsafe("g_browser.receive",
                                           *command_value.get());
  }
}

void NetInternalsMessageHandler::OnRendererReady(const base::ListValue* list) {
  IOThreadImpl::CallbackHelper(&IOThreadImpl::OnRendererReady, proxy_, list);
}

void NetInternalsMessageHandler::OnClearBrowserCache(
    const base::ListValue* list) {
  content::BrowsingDataRemover* remover =
      Profile::GetBrowsingDataRemover(Profile::FromWebUI(web_ui()));
  remover->Remove(base::Time(), base::Time::Max(),
                  content::BrowsingDataRemover::DATA_TYPE_CACHE,
                  content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB);
  // BrowsingDataRemover deletes itself.
}

void NetInternalsMessageHandler::OnGetPrerenderInfo(
    const base::ListValue* list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  SendJavascriptCommand(
      "receivedPrerenderInfo",
      chrome_browser_net::GetPrerenderInfo(Profile::FromWebUI(web_ui())));
}

void NetInternalsMessageHandler::OnGetHistoricNetworkStats(
    const base::ListValue* list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  SendJavascriptCommand("receivedHistoricNetworkStats",
                        chrome_browser_net::GetHistoricNetworkStats(
                            Profile::FromWebUI(web_ui())));
}

void NetInternalsMessageHandler::OnGetSessionNetworkStats(
    const base::ListValue* list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  SendJavascriptCommand(
      "receivedSessionNetworkStats",
      chrome_browser_net::GetSessionNetworkStats(Profile::FromWebUI(web_ui())));
}

void NetInternalsMessageHandler::OnGetExtensionInfo(
    const base::ListValue* list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  SendJavascriptCommand(
      "receivedExtensionInfo",
      chrome_browser_net::GetExtensionInfo(Profile::FromWebUI(web_ui())));
}

void NetInternalsMessageHandler::OnGetDataReductionProxyInfo(
    const base::ListValue* list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  SendJavascriptCommand("receivedDataReductionProxyInfo",
                        chrome_browser_net::GetDataReductionProxyInfo(
                            Profile::FromWebUI(web_ui())));
}

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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  AddRequestContextGetter(main_context_getter);
}

NetInternalsMessageHandler::IOThreadImpl::~IOThreadImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void NetInternalsMessageHandler::IOThreadImpl::AddRequestContextGetter(
    net::URLRequestContextGetter* context_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  context_getters_.push_back(context_getter);
}

void NetInternalsMessageHandler::IOThreadImpl::CallbackHelper(
    MessageHandler method,
    scoped_refptr<IOThreadImpl> io_thread,
    const base::ListValue* list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // We need to make a copy of the value in order to pass it over to the IO
  // thread. |list_copy| will be deleted when the task is destroyed. The called
  // |method| cannot take ownership of |list_copy|.
  base::ListValue* list_copy =
      (list && list->GetSize()) ? list->DeepCopy() : nullptr;

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(method, io_thread, base::Owned(list_copy)));
}

void NetInternalsMessageHandler::IOThreadImpl::Detach() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Unregister with network stack to observe events.
  if (net_log())
    net_log()->RemoveObserver(this);
}

void NetInternalsMessageHandler::IOThreadImpl::OnWebUIDeleted() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  was_webui_deleted_ = true;
}

void NetInternalsMessageHandler::IOThreadImpl::OnRendererReady(
    const base::ListValue* list) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // If currently watching the NetLog, temporarily stop watching it and flush
  // pending events, so they won't appear before the status events created for
  // currently active network objects below.
  if (net_log()) {
    net_log()->RemoveObserver(this);
    PostPendingEntries();
  }

  SendJavascriptCommand(
      "receivedConstants",
      net_log::ChromeNetLog::GetConstants(
          base::CommandLine::ForCurrentProcess()->GetCommandLineString(),
          chrome::GetChannelString()));

  PrePopulateEventList();

  // Register with network stack to observe events.
  io_thread_->net_log()->AddObserver(
      this, net::NetLogCaptureMode::IncludeCookiesAndCredentials());
}

void NetInternalsMessageHandler::IOThreadImpl::OnGetNetInfo(
    const base::ListValue* list) {
  DCHECK(list);
  int info_sources;
  if (!list->GetInteger(0, &info_sources))
    return;
  SendNetInfo(info_sources);
}

void NetInternalsMessageHandler::IOThreadImpl::OnReloadProxySettings(
    const base::ListValue* list) {
  DCHECK(!list);
  GetMainContext()->proxy_service()->ForceReloadProxyConfig();

  // Cause the renderer to be notified of the new values.
  SendNetInfo(net::NET_INFO_PROXY_SETTINGS);
}

void NetInternalsMessageHandler::IOThreadImpl::OnClearBadProxies(
    const base::ListValue* list) {
  DCHECK(!list);
  GetMainContext()->proxy_service()->ClearBadProxiesCache();

  // Cause the renderer to be notified of the new values.
  SendNetInfo(net::NET_INFO_BAD_PROXIES);
}

void NetInternalsMessageHandler::IOThreadImpl::OnClearHostResolverCache(
    const base::ListValue* list) {
  DCHECK(!list);
  net::HostCache* cache = GetHostResolverCache(GetMainContext());

  if (cache)
    cache->clear();

  // Cause the renderer to be notified of the new values.
  SendNetInfo(net::NET_INFO_HOST_RESOLVER);
}

void NetInternalsMessageHandler::IOThreadImpl::OnHSTSQuery(
    const base::ListValue* list) {
  // |list| should be: [<domain to query>].
  std::string domain;
  CHECK(list->GetString(0, &domain));
  auto result = base::MakeUnique<base::DictionaryValue>();

  if (base::IsStringASCII(domain)) {
    net::TransportSecurityState* transport_security_state =
        GetMainContext()->transport_security_state();
    if (transport_security_state) {
      net::TransportSecurityState::STSState static_sts_state;
      net::TransportSecurityState::PKPState static_pkp_state;
      const bool found_static = transport_security_state->GetStaticDomainState(
          domain, &static_sts_state, &static_pkp_state);
      if (found_static) {
        result->SetInteger("static_upgrade_mode",
                           static_cast<int>(static_sts_state.upgrade_mode));
        result->SetBoolean("static_sts_include_subdomains",
                           static_sts_state.include_subdomains);
        result->SetDouble("static_sts_observed",
                          static_sts_state.last_observed.ToDoubleT());
        result->SetDouble("static_sts_expiry",
                          static_sts_state.expiry.ToDoubleT());
        result->SetBoolean("static_pkp_include_subdomains",
                           static_pkp_state.include_subdomains);
        result->SetDouble("static_pkp_observed",
                          static_pkp_state.last_observed.ToDoubleT());
        result->SetDouble("static_pkp_expiry",
                          static_pkp_state.expiry.ToDoubleT());
        result->SetString("static_spki_hashes",
                          HashesToBase64String(static_pkp_state.spki_hashes));
        result->SetString("static_sts_domain", static_sts_state.domain);
        result->SetString("static_pkp_domain", static_pkp_state.domain);
      }

      net::TransportSecurityState::STSState dynamic_sts_state;
      net::TransportSecurityState::PKPState dynamic_pkp_state;
      const bool found_sts_dynamic =
          transport_security_state->GetDynamicSTSState(domain,
                                                       &dynamic_sts_state);

      const bool found_pkp_dynamic =
          transport_security_state->GetDynamicPKPState(domain,
                                                       &dynamic_pkp_state);
      if (found_sts_dynamic) {
        result->SetInteger("dynamic_upgrade_mode",
                           static_cast<int>(dynamic_sts_state.upgrade_mode));
        result->SetBoolean("dynamic_sts_include_subdomains",
                           dynamic_sts_state.include_subdomains);
        result->SetDouble("dynamic_sts_observed",
                          dynamic_sts_state.last_observed.ToDoubleT());
        result->SetDouble("dynamic_sts_expiry",
                          dynamic_sts_state.expiry.ToDoubleT());
        result->SetString("dynamic_sts_domain", dynamic_sts_state.domain);
      }

      if (found_pkp_dynamic) {
        result->SetBoolean("dynamic_pkp_include_subdomains",
                           dynamic_pkp_state.include_subdomains);
        result->SetDouble("dynamic_pkp_observed",
                          dynamic_pkp_state.last_observed.ToDoubleT());
        result->SetDouble("dynamic_pkp_expiry",
                          dynamic_pkp_state.expiry.ToDoubleT());
        result->SetString("dynamic_spki_hashes",
                          HashesToBase64String(dynamic_pkp_state.spki_hashes));
        result->SetString("dynamic_pkp_domain", dynamic_pkp_state.domain);
      }

      result->SetBoolean(
          "result", found_static || found_sts_dynamic || found_pkp_dynamic);
    } else {
      result->SetString("error", "no TransportSecurityState active");
    }
  } else {
    result->SetString("error", "non-ASCII domain name");
  }

  SendJavascriptCommand("receivedHSTSResult", std::move(result));
}

void NetInternalsMessageHandler::IOThreadImpl::OnHSTSAdd(
    const base::ListValue* list) {
  // |list| should be: [<domain to query>, <STS include subdomains>, <PKP
  // include subdomains>, <key pins>].
  std::string domain;
  CHECK(list->GetString(0, &domain));
  if (!base::IsStringASCII(domain)) {
    // Silently fail. The user will get a helpful error if they query for the
    // name.
    return;
  }
  bool sts_include_subdomains;
  CHECK(list->GetBoolean(1, &sts_include_subdomains));
  bool pkp_include_subdomains;
  CHECK(list->GetBoolean(2, &pkp_include_subdomains));
  std::string hashes_str;
  CHECK(list->GetString(3, &hashes_str));

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

  transport_security_state->AddHSTS(domain, expiry, sts_include_subdomains);
  transport_security_state->AddHPKP(domain, expiry, pkp_include_subdomains,
                                    hashes, GURL());
}

void NetInternalsMessageHandler::IOThreadImpl::OnHSTSDelete(
    const base::ListValue* list) {
  // |list| should be: [<domain to query>].
  std::string domain;
  CHECK(list->GetString(0, &domain));
  if (!base::IsStringASCII(domain)) {
    // There cannot be a unicode entry in the HSTS set.
    return;
  }
  net::TransportSecurityState* transport_security_state =
      GetMainContext()->transport_security_state();
  if (!transport_security_state)
    return;

  transport_security_state->DeleteDynamicDataForHost(domain);
}

void NetInternalsMessageHandler::IOThreadImpl::OnFlushSocketPools(
    const base::ListValue* list) {
  DCHECK(!list);
  net::HttpNetworkSession* http_network_session =
      GetHttpNetworkSession(GetMainContext());

  if (http_network_session)
    http_network_session->CloseAllConnections();
}

void NetInternalsMessageHandler::IOThreadImpl::OnCloseIdleSockets(
    const base::ListValue* list) {
  DCHECK(!list);
  net::HttpNetworkSession* http_network_session =
      GetHttpNetworkSession(GetMainContext());

  if (http_network_session)
    http_network_session->CloseIdleConnections();
}

#if defined(OS_WIN)
void NetInternalsMessageHandler::IOThreadImpl::OnGetServiceProviders(
    const base::ListValue* list) {
  DCHECK(!list);
  SendJavascriptCommand("receivedServiceProviders",
                        chrome_browser_net::GetWindowsServiceProviders());
}
#endif

#if defined(OS_CHROMEOS)
void NetInternalsMessageHandler::ImportONCFileToNSSDB(
    const std::string& onc_blob,
    const std::string& passcode,
    net::NSSCertDatabase* nssdb) {
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(
          Profile::FromWebUI(web_ui()));

  if (!user) {
    std::string error = "User not found.";
    SendJavascriptCommand("receivedONCFileParse",
                          base::MakeUnique<base::Value>(error));
    return;
  }

  std::string error;
  onc::ONCSource onc_source = onc::ONC_SOURCE_USER_IMPORT;
  base::ListValue network_configs;
  base::DictionaryValue global_network_config;
  base::ListValue certificates;
  if (!chromeos::onc::ParseAndValidateOncForImport(onc_blob,
                                                   onc_source,
                                                   passcode,
                                                   &network_configs,
                                                   &global_network_config,
                                                   &certificates)) {
    error = "Errors occurred during the ONC parsing. ";
  }

  std::string network_error;
  chromeos::onc::ImportNetworksForUser(user, network_configs, &network_error);
  if (!network_error.empty())
    error += network_error;

  chromeos::onc::CertificateImporterImpl cert_importer(
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO), nssdb);
  cert_importer.ImportCertificates(
      certificates,
      onc_source,
      base::Bind(&NetInternalsMessageHandler::OnCertificatesImported,
                 AsWeakPtr(),
                 error));
}

void NetInternalsMessageHandler::OnCertificatesImported(
    const std::string& previous_error,
    bool success,
    const net::CertificateList& /* unused onc_trusted_certificates */) {
  std::string error = previous_error;
  if (!success)
    error += "Some certificates couldn't be imported. ";

  SendJavascriptCommand("receivedONCFileParse",
                        base::MakeUnique<base::Value>(error));
}

void NetInternalsMessageHandler::OnImportONCFile(
    const base::ListValue* list) {
  std::string onc_blob;
  std::string passcode;
  if (list->GetSize() != 2 ||
      !list->GetString(0, &onc_blob) ||
      !list->GetString(1, &passcode)) {
    NOTREACHED();
  }

  GetNSSCertDatabaseForProfile(
      Profile::FromWebUI(web_ui()),
      base::Bind(&NetInternalsMessageHandler::ImportONCFileToNSSDB, AsWeakPtr(),
                 onc_blob, passcode));
}

void NetInternalsMessageHandler::OnStoreDebugLogs(const base::ListValue* list) {
  DCHECK(list);

  SendJavascriptCommand("receivedStoreDebugLogs",
                        base::MakeUnique<base::Value>("Creating log file..."));
  Profile* profile = Profile::FromWebUI(web_ui());
  const DownloadPrefs* const prefs = DownloadPrefs::FromBrowserContext(profile);
  base::FilePath path = prefs->DownloadPath();
  if (file_manager::util::IsUnderNonNativeLocalPath(profile, path))
    path = prefs->GetDefaultDownloadDirectoryForProfile();
  chromeos::DebugLogWriter::StoreLogs(
      path,
      true,  // should_compress
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
                        base::MakeUnique<base::Value>(status));
}

void NetInternalsMessageHandler::OnSetNetworkDebugMode(
    const base::ListValue* list) {
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
  std::string status = succeeded ? "Debug mode is changed to "
                                 : "Failed to change debug mode to ";
  status += subsystem;
  SendJavascriptCommand("receivedSetNetworkDebugMode",
                        base::MakeUnique<base::Value>(status));
}
#endif  // defined(OS_CHROMEOS)

void NetInternalsMessageHandler::IOThreadImpl::OnSetCaptureMode(
    const base::ListValue* list) {
  std::string capture_mode_string;
  if (!list->GetString(0, &capture_mode_string)) {
    NOTREACHED();
    return;
  }

  // Convert the string to a NetLogCaptureMode.
  net::NetLogCaptureMode mode;
  if (capture_mode_string == "IncludeSocketBytes") {
    mode = net::NetLogCaptureMode::IncludeSocketBytes();
  } else if (capture_mode_string == "IncludeCookiesAndCredentials") {
    mode = net::NetLogCaptureMode::IncludeCookiesAndCredentials();
  } else {
    NOTREACHED();
  }

  net_log()->SetObserverCaptureMode(this, mode);
}

// Note that unlike other methods of IOThreadImpl, this function
// can be called from ANY THREAD.
void NetInternalsMessageHandler::IOThreadImpl::OnAddEntry(
    const net::NetLogEntry& entry) {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::BindOnce(&IOThreadImpl::AddEntryToQueue, this,
                                         base::Passed(entry.ToValue())));
}

// Note that this can be called from ANY THREAD.
void NetInternalsMessageHandler::IOThreadImpl::SendJavascriptCommand(
    const std::string& command,
    std::unique_ptr<base::Value> arg) {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    if (handler_ && !was_webui_deleted_) {
      // We check |handler_| in case it was deleted on the UI thread earlier
      // while we were running on the IO thread.
      handler_->SendJavascriptCommand(command, std::move(arg));
    }
    return;
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::BindOnce(&IOThreadImpl::SendJavascriptCommand,
                                         this, command, base::Passed(&arg)));
}

void NetInternalsMessageHandler::IOThreadImpl::AddEntryToQueue(
    std::unique_ptr<base::Value> entry) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!pending_entries_) {
    pending_entries_.reset(new base::ListValue());
    BrowserThread::PostDelayedTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(&IOThreadImpl::PostPendingEntries, this),
        base::TimeDelta::FromMilliseconds(kNetLogEventDelayMilliseconds));
  }
  pending_entries_->Append(std::move(entry));
}

void NetInternalsMessageHandler::IOThreadImpl::PostPendingEntries() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (pending_entries_)
    SendJavascriptCommand("receivedLogEntries", std::move(pending_entries_));
}

void NetInternalsMessageHandler::IOThreadImpl::PrePopulateEventList() {
  // Using a set removes any duplicates.
  std::set<net::URLRequestContext*> contexts;
  for (const auto& getter : context_getters_)
    contexts.insert(getter->GetURLRequestContext());
  contexts.insert(io_thread_->globals()->system_request_context);

  // Add entries for ongoing network objects.
  CreateNetLogEntriesForActiveObjects(contexts, this);
}

void NetInternalsMessageHandler::IOThreadImpl::SendNetInfo(int info_sources) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SendJavascriptCommand("receivedNetInfo",
                        net::GetNetInfo(GetMainContext(), info_sources));
}

}  // namespace


////////////////////////////////////////////////////////////////////////////////
//
// NetInternalsUI
//
////////////////////////////////////////////////////////////////////////////////

NetInternalsUI::NetInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(base::MakeUnique<NetInternalsMessageHandler>());

  // Set up the chrome://net-internals/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateNetInternalsHTMLSource());
}
