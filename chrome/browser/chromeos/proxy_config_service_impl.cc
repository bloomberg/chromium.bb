// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/proxy_config_service_impl.h"

#include <ostream>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/values.h"
#include "chrome/browser/chrome_thread.h"

namespace chromeos {

namespace {

const char* SourceToString(ProxyConfigServiceImpl::ProxyConfig::Source source) {
  switch (source) {
    case ProxyConfigServiceImpl::ProxyConfig::SOURCE_NONE:
      return "SOURCE_NONE";
    case ProxyConfigServiceImpl::ProxyConfig::SOURCE_POLICY:
      return "SOURCE_POLICY";
    case ProxyConfigServiceImpl::ProxyConfig::SOURCE_OWNER:
      return "SOURCE_OWNER";
  }
  NOTREACHED() << "Unrecognized source type";
  return "";
}

std::ostream& operator<<(std::ostream& out,
    const ProxyConfigServiceImpl::ProxyConfig::ManualProxy& proxy) {
  out << "  " << SourceToString(proxy.source) << "\n"
      << "  server: " << (proxy.server.is_valid() ? proxy.server.ToURI() : "")
      << "\n";
  return out;
}

std::ostream& operator<<(std::ostream& out,
    const ProxyConfigServiceImpl::ProxyConfig& config) {
  switch (config.mode) {
    case ProxyConfigServiceImpl::ProxyConfig::MODE_DIRECT:
      out << "Direct connection:\n  "
          << SourceToString(config.automatic_proxy.source) << "\n";
      break;
    case ProxyConfigServiceImpl::ProxyConfig::MODE_AUTO_DETECT:
      out << "Auto detection:\n  "
          << SourceToString(config.automatic_proxy.source) << "\n";
      break;
    case ProxyConfigServiceImpl::ProxyConfig::MODE_PAC_SCRIPT:
      out << "Custom PAC script:\n  "
          << SourceToString(config.automatic_proxy.source)
          << "\n  PAC: " << config.automatic_proxy.pac_url << "\n";
      break;
    case ProxyConfigServiceImpl::ProxyConfig::MODE_SINGLE_PROXY:
      out << "Single proxy:\n" << config.single_proxy;
      break;
    case ProxyConfigServiceImpl::ProxyConfig::MODE_PROXY_PER_SCHEME:
      out << "HTTP proxy: " << config.http_proxy;
      out << "HTTPS proxy: " << config.https_proxy;
      out << "FTP proxy: " << config.ftp_proxy;
      out << "SOCKS proxy: " << config.socks_proxy;
      break;
    default:
      NOTREACHED() << "Unrecognized proxy config mode";
      break;
  }
  if (config.mode == ProxyConfigServiceImpl::ProxyConfig::MODE_SINGLE_PROXY ||
      config.mode ==
          ProxyConfigServiceImpl::ProxyConfig::MODE_PROXY_PER_SCHEME) {
    out << "Bypass list: ";
    if (config.bypass_rules.rules().empty()) {
      out << "[None]";
    } else {
      const net::ProxyBypassRules& bypass_rules = config.bypass_rules;
      net::ProxyBypassRules::RuleList::const_iterator it;
      for (it = bypass_rules.rules().begin();
           it != bypass_rules.rules().end(); ++it) {
        out << "\n    " << (*it)->ToString();
      }
    }
  }
  return out;
}

std::string ProxyConfigToString(
    const ProxyConfigServiceImpl::ProxyConfig& proxy_config) {
  std::ostringstream stream;
  stream << proxy_config;
  return stream.str();
}

}  // namespace

//-------------- ProxyConfigServiceImpl::ProxyConfig methods -------------------

bool ProxyConfigServiceImpl::ProxyConfig::Setting::CanBeWrittenByUser(
    bool user_is_owner) {
  // Setting can only be written by user if user is owner and setting is not
  // from policy.
  return user_is_owner && source != ProxyConfig::SOURCE_POLICY;
}

void ProxyConfigServiceImpl::ProxyConfig::ConvertToNetProxyConfig(
    net::ProxyConfig* net_config) {
  switch (mode) {
    case MODE_DIRECT:
      *net_config = net::ProxyConfig::CreateDirect();
      break;
    case MODE_AUTO_DETECT:
      *net_config = net::ProxyConfig::CreateAutoDetect();
      break;
    case MODE_PAC_SCRIPT:
      *net_config = net::ProxyConfig::CreateFromCustomPacURL(
          automatic_proxy.pac_url);
      break;
    case MODE_SINGLE_PROXY:
      *net_config = net::ProxyConfig();
      net_config->proxy_rules().type =
             net::ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY;
      net_config->proxy_rules().single_proxy = single_proxy.server;
      net_config->proxy_rules().bypass_rules = bypass_rules;
      break;
    case MODE_PROXY_PER_SCHEME:
      *net_config = net::ProxyConfig();
      net_config->proxy_rules().type =
          net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME;
      net_config->proxy_rules().proxy_for_http = http_proxy.server;
      net_config->proxy_rules().proxy_for_https = https_proxy.server;
      net_config->proxy_rules().proxy_for_ftp = ftp_proxy.server;
      net_config->proxy_rules().fallback_proxy = socks_proxy.server;
      net_config->proxy_rules().bypass_rules = bypass_rules;
      break;
    default:
      NOTREACHED() << "Unrecognized proxy config mode";
      break;
  }
}

std::string ProxyConfigServiceImpl::ProxyConfig::ToString() const {
  return ProxyConfigToString(*this);
}

//------------------- ProxyConfigServiceImpl: public methods -------------------

ProxyConfigServiceImpl::ProxyConfigServiceImpl() {
  // Fetch and cache proxy config from cros settings persisted on device.
  // TODO(kuan): retrieve config from policy and owner and merge them
  // for now, hardcode it as AUTO_DETECT and it'll pick up the PAC script set in
  // chromeos environment.
  cached_config_.mode = ProxyConfig::MODE_AUTO_DETECT;
  cached_config_.automatic_proxy.source = ProxyConfig::SOURCE_OWNER;

  // Update the thread-private copy in |reference_config_| as well.
  reference_config_ = cached_config_;
}

ProxyConfigServiceImpl::ProxyConfigServiceImpl(const ProxyConfig& init_config) {
  cached_config_ = init_config;
  // Update the thread-private copy in |reference_config_| as well.
  reference_config_ = cached_config_;
}

ProxyConfigServiceImpl::~ProxyConfigServiceImpl() {
}

void ProxyConfigServiceImpl::UIGetProxyConfig(ProxyConfig* config) {
  // Should be called from UI thread.
  CheckCurrentlyOnUIThread();
  // Simply returns the copy on the UI thread.
  *config = reference_config_;
}

void ProxyConfigServiceImpl::UISetProxyConfigToDirect() {
  // Should be called from UI thread.
  CheckCurrentlyOnUIThread();
  reference_config_.mode = ProxyConfig::MODE_DIRECT;
  OnUISetProxyConfig();
}

void ProxyConfigServiceImpl::UISetProxyConfigToAutoDetect() {
  // Should be called from UI thread.
  CheckCurrentlyOnUIThread();
  reference_config_.mode = ProxyConfig::MODE_AUTO_DETECT;
  OnUISetProxyConfig();
}

void ProxyConfigServiceImpl::UISetProxyConfigToPACScript(const GURL& url) {
  // Should be called from UI thread.
  CheckCurrentlyOnUIThread();
  reference_config_.mode = ProxyConfig::MODE_PAC_SCRIPT;
  reference_config_.automatic_proxy.pac_url = url;
  OnUISetProxyConfig();
}

void ProxyConfigServiceImpl::UISetProxyConfigToSingleProxy(
    const net::ProxyServer& server) {
  // Should be called from UI thread.
  CheckCurrentlyOnUIThread();
  reference_config_.mode = ProxyConfig::MODE_SINGLE_PROXY;
  reference_config_.single_proxy.server = server;
  OnUISetProxyConfig();
}

void ProxyConfigServiceImpl::UISetProxyConfigToProxyPerScheme(
    const std::string& scheme, const net::ProxyServer& server) {
  // Should be called from UI thread.
  CheckCurrentlyOnUIThread();
  ProxyConfig::ManualProxy* proxy = NULL;
  if (scheme == "http")
    proxy = &reference_config_.http_proxy;
  else if (scheme == "https")
    proxy = &reference_config_.https_proxy;
  else if (scheme == "ftp")
    proxy = &reference_config_.ftp_proxy;
  else if (scheme == "socks")
    proxy = &reference_config_.socks_proxy;
  else
    NOTREACHED() << "Unrecognized scheme for setting proxy config";
  if (proxy) {
    reference_config_.mode = ProxyConfig::MODE_PROXY_PER_SCHEME;
    proxy->server = server;
    OnUISetProxyConfig();
  }
}

void ProxyConfigServiceImpl::UISetProxyConfigBypassRules(
    const net::ProxyBypassRules& bypass_rules) {
  // Should be called from UI thread.
  CheckCurrentlyOnUIThread();
  DCHECK(reference_config_.mode == ProxyConfig::MODE_SINGLE_PROXY ||
         reference_config_.mode == ProxyConfig::MODE_PROXY_PER_SCHEME);
  if (reference_config_.mode == ProxyConfig::MODE_SINGLE_PROXY ||
      reference_config_.mode == ProxyConfig::MODE_PROXY_PER_SCHEME) {
    reference_config_.bypass_rules = bypass_rules;
    OnUISetProxyConfig();
  }
}

void ProxyConfigServiceImpl::AddObserver(
    net::ProxyConfigService::Observer* observer) {
  // Should be called from IO thread.
  CheckCurrentlyOnIOThread();
  observers_.AddObserver(observer);
}

void ProxyConfigServiceImpl::RemoveObserver(
    net::ProxyConfigService::Observer* observer) {
  // Should be called from IO thread.
  CheckCurrentlyOnIOThread();
  observers_.RemoveObserver(observer);
}

bool ProxyConfigServiceImpl::IOGetProxyConfig(net::ProxyConfig* net_config) {
  // Should be called from IO thread.
  CheckCurrentlyOnIOThread();

  // Simply return the last cached proxy configuration.
  cached_config_.ConvertToNetProxyConfig(net_config);

  // We return true to indicate that *config was filled in. It is always
  // going to be available since we initialized eagerly on the UI thread.
  // TODO(kuan): do lazy initialization instead, so we no longer need
  //             to construct ProxyConfigServiceImpl on the UI thread.
  //             In which case, we may return false here.
  return true;
}

//------------------ ProxyConfigServiceImpl: private methods -------------------

void ProxyConfigServiceImpl::OnUISetProxyConfig() {
  // Posts a task to IO thread with the new config, so it can update
  // |cached_config_|.
  Task* task = NewRunnableMethod(this,
      &ProxyConfigServiceImpl::IOSetProxyConfig, reference_config_);
  if (!ChromeThread::PostTask(ChromeThread::IO, FROM_HERE, task)) {
    LOG(INFO) << "Couldn't post task to IO thread to set new proxy config";
    delete task;
  }

  // TODO(kuan): write new config out to cros settings for persistence.
}

void ProxyConfigServiceImpl::CheckCurrentlyOnIOThread() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
}

void ProxyConfigServiceImpl::CheckCurrentlyOnUIThread() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
}

void ProxyConfigServiceImpl::IOSetProxyConfig(const ProxyConfig& new_config) {
  // This is called on the IO thread (posted from UI thread).
  CheckCurrentlyOnIOThread();
  LOG(INFO) << "Proxy configuration changed";
  cached_config_ = new_config;
  // Notify observers of new proxy config.
  net::ProxyConfig net_config;
  cached_config_.ConvertToNetProxyConfig(&net_config);
  FOR_EACH_OBSERVER(net::ProxyConfigService::Observer, observers_,
                    OnProxyConfigChanged(net_config));
}

}  // namespace chromeos
