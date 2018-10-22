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
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/net_internals_resources.h"
#include "components/onc/onc_constants.h"
#include "components/prefs/pref_member.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "net/base/net_errors.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/transport_security_state.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/expect_ct_reporter.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/file_manager/filesystem_api_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/system_logs/debug_log_writer.h"
#include "chrome/browser/net/nss_context.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "chromeos/network/onc/onc_certificate_importer_impl.h"
#include "chromeos/network/onc/onc_parsed_certificates.h"
#include "chromeos/network/onc/onc_utils.h"
#endif

using base::Value;
using content::BrowserThread;
using content::WebContents;
using content::WebUIMessageHandler;

namespace {

// A Base-64 encoded DER certificate for use in test Expect-CT reports. The
// contents of the certificate don't matter.
const char kTestReportCert[] =
    "MIIDvzCCAqegAwIBAgIBAzANBgkqhkiG9w0BAQsFADBjMQswCQYDVQQGEwJVUzET"
    "MBEGA1UECAwKQ2FsaWZvcm5pYTEWMBQGA1UEBwwNTW91bnRhaW4gVmlldzEQMA4G"
    "A1UECgwHVGVzdCBDQTEVMBMGA1UEAwwMVGVzdCBSb290IENBMB4XDTE3MDYwNTE3"
    "MTA0NloXDTI3MDYwMzE3MTA0NlowYDELMAkGA1UEBhMCVVMxEzARBgNVBAgMCkNh"
    "bGlmb3JuaWExFjAUBgNVBAcMDU1vdW50YWluIFZpZXcxEDAOBgNVBAoMB1Rlc3Qg"
    "Q0ExEjAQBgNVBAMMCTEyNy4wLjAuMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCC"
    "AQoCggEBALS/0pcz5RNbd2W9cxp1KJtHWea3MOhGM21YW9ofCv/k5C3yHfiJ6GQu"
    "9sPN16OO1/fN59gOEMPnVtL85ebTTuL/gk0YY4ewo97a7wo3e6y1t0PO8gc53xTp"
    "w6RBPn5oRzSbe2HEGOYTzrO0puC6A+7k6+eq9G2+l1uqBpdQAdB4uNaSsOTiuUOI"
    "ta4UZH1ScNQFHAkl1eJPyaiC20Exw75EbwvU/b/B7tlivzuPtQDI0d9dShOtceRL"
    "X9HZckyD2JNAv2zNL2YOBNa5QygkySX9WXD+PfKpCk7Cm8TenldeXRYl5ni2REkp"
    "nfa/dPuF1g3xZVjyK9aPEEnIAC2I4i0CAwEAAaOBgDB+MAwGA1UdEwEB/wQCMAAw"
    "HQYDVR0OBBYEFODc4C8HiHQ6n9Mwo3GK+dal5aZTMB8GA1UdIwQYMBaAFJsmC4qY"
    "qbsduR8c4xpAM+2OF4irMB0GA1UdJQQWMBQGCCsGAQUFBwMBBggrBgEFBQcDAjAP"
    "BgNVHREECDAGhwR/AAABMA0GCSqGSIb3DQEBCwUAA4IBAQB6FEQuUDRcC5jkX3aZ"
    "uuTeZEqMVL7JXgvgFqzXsPb8zIdmxr/tEDfwXx2qDf2Dpxts7Fq4vqUwimK4qV3K"
    "7heLnWV2+FBvV1eeSfZ7AQj+SURkdlyo42r41+t13QUf+Z0ftR9266LSWLKrukeI"
    "Mxk73hOkm/u8enhTd00dy/FN9dOFBFHseVMspWNxIkdRILgOmiyfQNRgxNYdOf0e"
    "EfELR8Hn6WjZ8wAbvO4p7RTrzu1c/RZ0M+NLkID56Brbl70GC2h5681LPwAOaZ7/"
    "mWQ5kekSyJjmLfF12b+h9RVAt5MrXZgk2vNujssgGf4nbWh4KZyQ6qrs778ZdDLm"
    "yfUn";

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
  source->OverrideContentSecurityPolicyScriptSrc(
      "script-src chrome://resources 'self' 'unsafe-eval';");

  source->SetDefaultResource(IDR_NET_INTERNALS_INDEX_HTML);
  source->AddResourcePath("index.js", IDR_NET_INTERNALS_INDEX_JS);
  source->SetJsonPath("strings.js");
  source->UseGzip();
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
  void OnCertificatesImported(const std::string& previous_error,
                              bool cert_import_success);
#endif

 private:
  class IOThreadImpl;

  // This is the "real" message handler, which lives on the IO thread.
  scoped_refptr<IOThreadImpl> proxy_;

  DISALLOW_COPY_AND_ASSIGN(NetInternalsMessageHandler);
};

// This class is the "real" message handler. It is allocated and destroyed on
// the UI thread.  With the exception of OnWebUIDeleted, and
// SendJavascriptCommand, its methods are all expected to be called from the IO
// thread.  SendJavascriptCommand can be called from any thread, and
// OnWebUIDeleted can only be called from the UI thread.
class NetInternalsMessageHandler::IOThreadImpl
    : public base::RefCountedThreadSafe<
          NetInternalsMessageHandler::IOThreadImpl,
          BrowserThread::DeleteOnUIThread> {
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

  // Helper method to enable a callback that will be executed on the IO thread.
  static void CallbackHelper(MessageHandler method,
                             scoped_refptr<IOThreadImpl> io_thread,
                             const base::ListValue* list);

  // Called when the WebUI is deleted.  Prevents calling Javascript functions
  // afterwards.  Called on UI thread.
  void OnWebUIDeleted();

  //--------------------------------
  // Javascript message handlers:
  //--------------------------------

  void OnReloadProxySettings(const base::ListValue* list);
  void OnClearBadProxies(const base::ListValue* list);
  void OnClearHostResolverCache(const base::ListValue* list);
  void OnDomainSecurityPolicyDelete(const base::ListValue* list);
  void OnHSTSQuery(const base::ListValue* list);
  void OnHSTSAdd(const base::ListValue* list);
  void OnExpectCTQuery(const base::ListValue* list);
  void OnExpectCTAdd(const base::ListValue* list);
  void OnExpectCTTestReport(const base::ListValue* list);
  void OnCloseIdleSockets(const base::ListValue* list);
  void OnFlushSocketPools(const base::ListValue* list);

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

  ~IOThreadImpl();

  net::URLRequestContext* GetMainContext() {
    return main_context_getter_->GetURLRequestContext();
  }

  // Pointer to the UI-thread message handler. Only access this from
  // the UI thread.
  base::WeakPtr<NetInternalsMessageHandler> handler_;

  // The global IOThread.
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

  // Used to send test Expect-CT reports. Lazily initialized when the first
  // Expect-CT test report is sent. This is a member variable rather than a
  // local variable so that it lives long enough to receive the result of
  // sending a report, which is delivered to the JavaScript via a JavaScript
  // command.
  std::unique_ptr<network::ExpectCTReporter> expect_ct_reporter_;

  DISALLOW_COPY_AND_ASSIGN(IOThreadImpl);
};

////////////////////////////////////////////////////////////////////////////////
//
// NetInternalsMessageHandler
//
////////////////////////////////////////////////////////////////////////////////

NetInternalsMessageHandler::NetInternalsMessageHandler() {}

NetInternalsMessageHandler::~NetInternalsMessageHandler() {
  if (proxy_)
    proxy_->OnWebUIDeleted();
}

void NetInternalsMessageHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Profile* profile = Profile::FromWebUI(web_ui());

  proxy_ = new IOThreadImpl(this->AsWeakPtr(), g_browser_process->io_thread(),
                            profile->GetRequestContext());

  web_ui()->RegisterMessageCallback(
      "reloadProxySettings",
      base::BindRepeating(&IOThreadImpl::CallbackHelper,
                          &IOThreadImpl::OnReloadProxySettings, proxy_));
  web_ui()->RegisterMessageCallback(
      "clearBadProxies",
      base::BindRepeating(&IOThreadImpl::CallbackHelper,
                          &IOThreadImpl::OnClearBadProxies, proxy_));
  web_ui()->RegisterMessageCallback(
      "clearHostResolverCache",
      base::BindRepeating(&IOThreadImpl::CallbackHelper,
                          &IOThreadImpl::OnClearHostResolverCache, proxy_));
  web_ui()->RegisterMessageCallback(
      "domainSecurityPolicyDelete",
      base::BindRepeating(&IOThreadImpl::CallbackHelper,
                          &IOThreadImpl::OnDomainSecurityPolicyDelete, proxy_));
  web_ui()->RegisterMessageCallback(
      "hstsQuery", base::BindRepeating(&IOThreadImpl::CallbackHelper,
                                       &IOThreadImpl::OnHSTSQuery, proxy_));
  web_ui()->RegisterMessageCallback(
      "hstsAdd", base::BindRepeating(&IOThreadImpl::CallbackHelper,
                                     &IOThreadImpl::OnHSTSAdd, proxy_));
  web_ui()->RegisterMessageCallback(
      "expectCTQuery",
      base::BindRepeating(&IOThreadImpl::CallbackHelper,
                          &IOThreadImpl::OnExpectCTQuery, proxy_));
  web_ui()->RegisterMessageCallback(
      "expectCTAdd", base::BindRepeating(&IOThreadImpl::CallbackHelper,
                                         &IOThreadImpl::OnExpectCTAdd, proxy_));
  web_ui()->RegisterMessageCallback(
      "expectCTTestReport",
      base::BindRepeating(&IOThreadImpl::CallbackHelper,
                          &IOThreadImpl::OnExpectCTTestReport, proxy_));
  web_ui()->RegisterMessageCallback(
      "closeIdleSockets",
      base::BindRepeating(&IOThreadImpl::CallbackHelper,
                          &IOThreadImpl::OnCloseIdleSockets, proxy_));
  web_ui()->RegisterMessageCallback(
      "flushSocketPools",
      base::BindRepeating(&IOThreadImpl::CallbackHelper,
                          &IOThreadImpl::OnFlushSocketPools, proxy_));
#if defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      "importONCFile",
      base::BindRepeating(&NetInternalsMessageHandler::OnImportONCFile,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "storeDebugLogs",
      base::BindRepeating(&NetInternalsMessageHandler::OnStoreDebugLogs,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setNetworkDebugMode",
      base::BindRepeating(&NetInternalsMessageHandler::OnSetNetworkDebugMode,
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
}

NetInternalsMessageHandler::IOThreadImpl::~IOThreadImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(method, io_thread, base::Owned(list_copy)));
}

void NetInternalsMessageHandler::IOThreadImpl::OnWebUIDeleted() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  was_webui_deleted_ = true;
}

void NetInternalsMessageHandler::IOThreadImpl::OnReloadProxySettings(
    const base::ListValue* list) {
  DCHECK(!list);
  GetMainContext()->proxy_resolution_service()->ForceReloadProxyConfig();
}

void NetInternalsMessageHandler::IOThreadImpl::OnClearBadProxies(
    const base::ListValue* list) {
  DCHECK(!list);
  GetMainContext()->proxy_resolution_service()->ClearBadProxiesCache();
}

void NetInternalsMessageHandler::IOThreadImpl::OnClearHostResolverCache(
    const base::ListValue* list) {
  DCHECK(!list);
  net::HostCache* cache = GetHostResolverCache(GetMainContext());

  if (cache)
    cache->clear();
}

void NetInternalsMessageHandler::IOThreadImpl::OnDomainSecurityPolicyDelete(
    const base::ListValue* list) {
  // |list| should be: [<domain to query>].
  std::string domain;
  bool result = list->GetString(0, &domain);
  DCHECK(result);
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

void NetInternalsMessageHandler::IOThreadImpl::OnHSTSQuery(
    const base::ListValue* list) {
  // |list| should be: [<domain to query>].
  std::string domain;
  bool get_domain_result = list->GetString(0, &domain);
  DCHECK(get_domain_result);
  auto result = std::make_unique<base::DictionaryValue>();

  if (base::IsStringASCII(domain)) {
    net::TransportSecurityState* transport_security_state =
        GetMainContext()->transport_security_state();
    if (transport_security_state) {
      net::TransportSecurityState::STSState static_sts_state;
      net::TransportSecurityState::PKPState static_pkp_state;
      bool found_static = transport_security_state->GetStaticDomainState(
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
      bool found_sts_dynamic = transport_security_state->GetDynamicSTSState(
          domain, &dynamic_sts_state);

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

      result->SetBoolean("result", found_static || found_sts_dynamic);
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
  // |list| should be: [<domain to query>, <STS include subdomains>]
  std::string domain;
  bool result = list->GetString(0, &domain);
  DCHECK(result);
  if (!base::IsStringASCII(domain)) {
    // Silently fail. The user will get a helpful error if they query for the
    // name.
    return;
  }
  bool sts_include_subdomains;
  result = list->GetBoolean(1, &sts_include_subdomains);
  DCHECK(result);

  net::TransportSecurityState* transport_security_state =
      GetMainContext()->transport_security_state();
  if (!transport_security_state)
    return;

  base::Time expiry = base::Time::Now() + base::TimeDelta::FromDays(1000);
  transport_security_state->AddHSTS(domain, expiry, sts_include_subdomains);
}

void NetInternalsMessageHandler::IOThreadImpl::OnExpectCTQuery(
    const base::ListValue* list) {
  // |list| should be: [<domain to query>].
  std::string domain;
  bool domain_result = list->GetString(0, &domain);
  DCHECK(domain_result);
  auto result = std::make_unique<base::DictionaryValue>();

  if (base::IsStringASCII(domain)) {
    net::TransportSecurityState* transport_security_state =
        GetMainContext()->transport_security_state();
    if (transport_security_state) {
      net::TransportSecurityState::ExpectCTState dynamic_expect_ct_state;
      bool found = transport_security_state->GetDynamicExpectCTState(
          domain, &dynamic_expect_ct_state);

      // TODO(estark): query static Expect-CT state as well.
      if (found) {
        result->SetString("dynamic_expect_ct_domain", domain);
        result->SetDouble("dynamic_expect_ct_observed",
                          dynamic_expect_ct_state.last_observed.ToDoubleT());
        result->SetDouble("dynamic_expect_ct_expiry",
                          dynamic_expect_ct_state.expiry.ToDoubleT());
        result->SetBoolean("dynamic_expect_ct_enforce",
                           dynamic_expect_ct_state.enforce);
        result->SetString("dynamic_expect_ct_report_uri",
                          dynamic_expect_ct_state.report_uri.spec());
      }

      result->SetBoolean("result", found);
    } else {
      result->SetString("error", "no Expect-CT state active");
    }
  } else {
    result->SetString("error", "non-ASCII domain name");
  }

  SendJavascriptCommand("receivedExpectCTResult", std::move(result));
}

void NetInternalsMessageHandler::IOThreadImpl::OnExpectCTAdd(
    const base::ListValue* list) {
  // |list| should be: [<domain to add>, <report URI>, <enforce>].
  std::string domain;
  bool result = list->GetString(0, &domain);
  DCHECK(result);
  if (!base::IsStringASCII(domain)) {
    // Silently fail. The user will get a helpful error if they query for the
    // name.
    return;
  }
  std::string report_uri_str;
  result = list->GetString(1, &report_uri_str);
  DCHECK(result);
  bool enforce;
  result = list->GetBoolean(2, &enforce);
  DCHECK(result);

  net::TransportSecurityState* transport_security_state =
      GetMainContext()->transport_security_state();
  if (!transport_security_state)
    return;

  base::Time expiry = base::Time::Now() + base::TimeDelta::FromDays(1000);
  transport_security_state->AddExpectCT(domain, expiry, enforce,
                                        GURL(report_uri_str));
}

void NetInternalsMessageHandler::IOThreadImpl::OnExpectCTTestReport(
    const base::ListValue* list) {
  // |list| should be: [<report URI>].
  std::string report_uri_str;
  bool result = list->GetString(0, &report_uri_str);
  DCHECK(result);
  GURL report_uri(report_uri_str);
  if (!report_uri.is_valid())
    return;

  std::string decoded_dummy_cert;
  DCHECK(base::Base64Decode(kTestReportCert, &decoded_dummy_cert));
  scoped_refptr<net::X509Certificate> dummy_cert =
      net::X509Certificate::CreateFromBytes(decoded_dummy_cert.data(),
                                            decoded_dummy_cert.size());
  net::SignedCertificateTimestampAndStatusList dummy_sct_list;

  if (!expect_ct_reporter_) {
    std::unique_ptr<base::Value> success =
        std::make_unique<base::Value>("success");
    std::unique_ptr<base::Value> failure =
        std::make_unique<base::Value>("failure");
    expect_ct_reporter_ = std::make_unique<network::ExpectCTReporter>(
        GetMainContext(),
        base::Bind(
            &NetInternalsMessageHandler::IOThreadImpl::SendJavascriptCommand,
            this, "receivedExpectCTTestReportResult", base::Passed(&success)),
        base::Bind(
            &NetInternalsMessageHandler::IOThreadImpl::SendJavascriptCommand,
            this, "receivedExpectCTTestReportResult", base::Passed(&failure)));
  }

  // Send a test report with dummy data.
  expect_ct_reporter_->OnExpectCTFailed(
      net::HostPortPair("expect-ct-report.test", 443), report_uri,
      base::Time::Now(), dummy_cert.get(), dummy_cert.get(), dummy_sct_list);
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
                          std::make_unique<base::Value>(error));
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
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}), nssdb);
  auto certs =
      std::make_unique<chromeos::onc::OncParsedCertificates>(certificates);
  if (certs->has_error())
    error += "Some certificates couldn't be parsed. ";
  cert_importer.ImportAllCertificatesUserInitiated(
      certs->server_or_authority_certificates(), certs->client_certificates(),
      base::Bind(&NetInternalsMessageHandler::OnCertificatesImported,
                 AsWeakPtr(), error /* previous_error */));
}

void NetInternalsMessageHandler::OnCertificatesImported(
    const std::string& previous_error,
    bool cert_import_success) {
  std::string error = previous_error;
  if (!cert_import_success)
    error += "Some certificates couldn't be imported. ";

  SendJavascriptCommand("receivedONCFileParse",
                        std::make_unique<base::Value>(error));
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
                        std::make_unique<base::Value>("Creating log file..."));
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
                        std::make_unique<base::Value>(status));
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
                        std::make_unique<base::Value>(status));
}
#endif  // defined(OS_CHROMEOS)

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

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(&IOThreadImpl::SendJavascriptCommand,
                                          this, command, std::move(arg)));
}

}  // namespace


////////////////////////////////////////////////////////////////////////////////
//
// NetInternalsUI
//
////////////////////////////////////////////////////////////////////////////////

NetInternalsUI::NetInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<NetInternalsMessageHandler>());

  // Set up the chrome://net-internals/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateNetInternalsHTMLSource());
}
