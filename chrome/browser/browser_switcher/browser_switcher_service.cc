// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_service.h"

#include <string>
#include <utility>

#include "build/build_config.h"
#include "chrome/browser/browser_switcher/alternative_browser_driver.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "chrome/browser/browser_switcher/browser_switcher_sitelist.h"
#include "chrome/browser/browser_switcher/ieem_sitelist_parser.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/simple_url_loader.h"

#if defined(OS_WIN)
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#endif

namespace browser_switcher {

namespace {

#if defined(OS_WIN)
const wchar_t kIeSiteListKey[] =
    L"SOFTWARE\\Policies\\Microsoft\\Internet Explorer\\Main\\EnterpriseMode";
const wchar_t kIeSiteListValue[] = L"SiteList";
#endif

// How long to wait after |BrowserSwitcherService| is created before initiating
// the sitelist fetch.
const base::TimeDelta kFetchSitelistDelay = base::TimeDelta::FromSeconds(60);

// How many times to re-try fetching the XML file for the sitelist.
const int kFetchNumRetries = 1;

// TODO(nicolaso): Add chrome_policy for this annotation once the policy is
// implemented.
constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation("browser_switcher_ieem_sitelist", R"(
        semantics {
          sender: "Browser Switcher"
          description:
            "BrowserSwitcher may download Internet Explorer's Enterprise Mode "
            "SiteList XML, to load the list of URLs to open in an alternative "
            "browser. This is often on the organization's intranet.For more "
            "information on Internet Explorer's Enterprise Mode, see: "
            "https://docs.microsoft.com/internet-explorer/ie11-deploy-guide"
            "/what-is-enterprise-mode"
          trigger:
            "This happens only once per profile, 60s after the first page "
            "starts loading. The request may be retried once if it failed the "
            "first time."
          data:
            "Up to 2 (plus retries) HTTP or HTTPS GET requests to the URLs "
            "configured in Internet Explorer's SiteList policy, and Chrome's "
            "BrowserSwitcherExternalSitelistUrl policy."
          destination: OTHER
          destination_other:
            "URL configured in Internet Explorer's SiteList policy, and URL "
            "configured in Chrome's BrowserSwitcherExternalSitelistUrl policy. "
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings."
          policy_exception_justification:
            "This feature  still in development, and is disabled by default. "
            "It needs to be enabled through policies."
        })");

}  // namespace

class XmlDownloader {
 public:
  // Posts a task to start downloading+parsing the rules after |delay|. Calls
  // |done_callback| when done, so the caller can apply the parsed rules and
  // clean up this object.
  XmlDownloader(Profile* profile,
                GURL url,
                base::TimeDelta delay,
                base::OnceCallback<void(ParsedXml)> done_callback);
  virtual ~XmlDownloader() = default;

 private:
  void FetchXml();
  void ParseXml(std::unique_ptr<std::string> bytes);
  void DoneParsing(ParsedXml xml);

  GURL url_;
  scoped_refptr<network::SharedURLLoaderFactory> factory_;

  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  base::OnceCallback<void(ParsedXml)> done_callback_;

  base::WeakPtrFactory<XmlDownloader> weak_ptr_factory_;
};

XmlDownloader::XmlDownloader(Profile* profile,
                             GURL url,
                             base::TimeDelta delay,
                             base::OnceCallback<void(ParsedXml)> done_callback)
    : url_(std::move(url)),
      done_callback_(std::move(done_callback)),
      weak_ptr_factory_(this) {
  factory_ = content::BrowserContext::GetDefaultStoragePartition(profile)
                 ->GetURLLoaderFactoryForBrowserProcess();
  DCHECK(factory_);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&XmlDownloader::FetchXml, weak_ptr_factory_.GetWeakPtr()),
      delay);
}

void XmlDownloader::FetchXml() {
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url_;
  request->load_flags =
      (net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES |
       net::LOAD_DISABLE_CACHE);
  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  url_loader_->SetRetryOptions(
      kFetchNumRetries,
      network::SimpleURLLoader::RetryMode::RETRY_ON_NETWORK_CHANGE);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      factory_.get(),
      base::BindOnce(&XmlDownloader::ParseXml, weak_ptr_factory_.GetWeakPtr()));
}

void XmlDownloader::ParseXml(std::unique_ptr<std::string> bytes) {
  if (!bytes) {
    DoneParsing(ParsedXml({}, {}, "could not fetch XML"));
    return;
  }
  ParseIeemXml(*bytes, base::BindOnce(&XmlDownloader::DoneParsing,
                                      weak_ptr_factory_.GetWeakPtr()));
}

void XmlDownloader::DoneParsing(ParsedXml xml) {
  factory_.reset();
  url_loader_.reset();
  std::move(done_callback_).Run(std::move(xml));
}

BrowserSwitcherService::BrowserSwitcherService(Profile* profile)
    : prefs_(profile->GetPrefs()),
      driver_(new AlternativeBrowserDriverImpl(&prefs_)),
      sitelist_(new BrowserSwitcherSitelistImpl(&prefs_)),
      weak_ptr_factory_(this) {
  GURL external_url = prefs_.GetExternalSitelistUrl();
  if (external_url.is_valid()) {
    external_sitelist_downloader_ = std::make_unique<XmlDownloader>(
        profile, std::move(external_url), fetch_delay_,
        base::BindOnce(&BrowserSwitcherService::OnExternalSitelistParsed,
                       weak_ptr_factory_.GetWeakPtr()));
  }

#if defined(OS_WIN)
  if (prefs_.UseIeSitelist()) {
    GURL sitelist_url = GetIeemSitelistUrl();
    if (sitelist_url.is_valid()) {
      ieem_downloader_ = std::make_unique<XmlDownloader>(
          profile, std::move(sitelist_url), fetch_delay_,
          base::BindOnce(&BrowserSwitcherService::OnIeemSitelistParsed,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }
#endif
}

BrowserSwitcherService::~BrowserSwitcherService() {}

AlternativeBrowserDriver* BrowserSwitcherService::driver() {
  return driver_.get();
}

BrowserSwitcherSitelist* BrowserSwitcherService::sitelist() {
  return sitelist_.get();
}

const BrowserSwitcherPrefs& BrowserSwitcherService::prefs() const {
  return prefs_;
}

void BrowserSwitcherService::SetDriverForTesting(
    std::unique_ptr<AlternativeBrowserDriver> driver) {
  driver_ = std::move(driver);
}

void BrowserSwitcherService::SetSitelistForTesting(
    std::unique_ptr<BrowserSwitcherSitelist> sitelist) {
  sitelist_ = std::move(sitelist);
}

void BrowserSwitcherService::OnExternalSitelistParsed(ParsedXml xml) {
  if (xml.error) {
    LOG(ERROR) << "Unable to parse IEEM SiteList: " << *xml.error;
  } else {
    VLOG(2) << "Done parsing external SiteList. "
            << "Applying rules to future navigations.";
    sitelist()->SetExternalSitelist(std::move(xml));
  }
  external_sitelist_downloader_.reset();
}

base::TimeDelta BrowserSwitcherService::fetch_delay_ = kFetchSitelistDelay;

// static
void BrowserSwitcherService::SetFetchDelayForTesting(base::TimeDelta delay) {
  fetch_delay_ = delay;
}

#if defined(OS_WIN)
base::Optional<std::string>
    BrowserSwitcherService::ieem_sitelist_url_for_testing_;

// static
void BrowserSwitcherService::SetIeemSitelistUrlForTesting(
    const std::string& spec) {
  ieem_sitelist_url_for_testing_ = spec;
}

// static
GURL BrowserSwitcherService::GetIeemSitelistUrl() {
  if (ieem_sitelist_url_for_testing_ != base::nullopt)
    return GURL(*ieem_sitelist_url_for_testing_);

  base::win::RegKey key;
  if (ERROR_SUCCESS != key.Open(HKEY_LOCAL_MACHINE, kIeSiteListKey, KEY_READ) &&
      ERROR_SUCCESS != key.Open(HKEY_CURRENT_USER, kIeSiteListKey, KEY_READ)) {
    return GURL();
  }
  std::wstring url_string;
  if (ERROR_SUCCESS != key.ReadValue(kIeSiteListValue, &url_string))
    return GURL();
  return GURL(base::UTF16ToUTF8(url_string));
}

void BrowserSwitcherService::OnIeemSitelistParsed(ParsedXml xml) {
  if (xml.error) {
    LOG(ERROR) << "Unable to parse IEEM SiteList: " << *xml.error;
  } else {
    VLOG(2) << "Done parsing IEEM SiteList. "
            << "Applying rules to future navigations.";
    sitelist()->SetIeemSitelist(std::move(xml));
  }
  ieem_downloader_.reset();
}
#endif

}  // namespace browser_switcher
