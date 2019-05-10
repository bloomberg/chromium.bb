// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_service.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/syslog_logging.h"
#include "chrome/browser/browser_switcher/alternative_browser_driver.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "chrome/browser/browser_switcher/browser_switcher_sitelist.h"
#include "chrome/browser/browser_switcher/ieem_sitelist_parser.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/file_url_loader.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace browser_switcher {

namespace {

// How long to wait after |BrowserSwitcherService| is created before initiating
// the sitelist fetch.
const base::TimeDelta kFetchSitelistDelay = base::TimeDelta::FromSeconds(60);

// How long to wait after a fetch to re-fetch the sitelist to keep it fresh.
const base::TimeDelta kRefreshSitelistDelay = base::TimeDelta::FromMinutes(30);

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

RulesetSource::RulesetSource(
    GURL url_,
    bool contains_inverted_rules_,
    base::OnceCallback<void(ParsedXml xml)> parsed_callback_)
    : url(std::move(url_)),
      contains_inverted_rules(contains_inverted_rules_),
      parsed_callback(std::move(parsed_callback_)) {}

RulesetSource::RulesetSource(RulesetSource&&) = default;

RulesetSource::~RulesetSource() = default;

XmlDownloader::XmlDownloader(Profile* profile,
                             std::vector<RulesetSource> sources,
                             base::OnceCallback<void()> all_done_callback)
    : sources_(std::move(sources)),
      all_done_callback_(std::move(all_done_callback)),
      weak_ptr_factory_(this) {
  file_url_factory_ =
      content::CreateFileURLLoaderFactory(base::FilePath(), nullptr);
  other_url_factory_ =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess();
  FetchXml();
}

XmlDownloader::~XmlDownloader() = default;

void XmlDownloader::FetchXml() {
  for (auto& source : sources_) {
    auto request = std::make_unique<network::ResourceRequest>();
    request->url = source.url;
    request->load_flags =
        (net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES |
         net::LOAD_DISABLE_CACHE);
    source.url_loader = network::SimpleURLLoader::Create(std::move(request),
                                                         traffic_annotation);
    source.url_loader->SetRetryOptions(
        kFetchNumRetries,
        network::SimpleURLLoader::RetryMode::RETRY_ON_NETWORK_CHANGE);
    source.url_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        GetURLLoaderFactoryForURL(source.url),
        base::BindOnce(&XmlDownloader::ParseXml, weak_ptr_factory_.GetWeakPtr(),
                       base::Unretained(&source)));
  }
}

network::mojom::URLLoaderFactory* XmlDownloader::GetURLLoaderFactoryForURL(
    const GURL& url) {
  if (url.SchemeIsFile())
    return file_url_factory_.get();
  return other_url_factory_.get();
}

void XmlDownloader::ParseXml(RulesetSource* source,
                             std::unique_ptr<std::string> bytes) {
  if (!bytes) {
    DoneParsing(source, ParsedXml({}, "could not fetch XML"));
    return;
  }
  ParseIeemXml(*bytes, base::BindOnce(&XmlDownloader::DoneParsing,
                                      weak_ptr_factory_.GetWeakPtr(),
                                      base::Unretained(source)));
}

void XmlDownloader::DoneParsing(RulesetSource* source, ParsedXml xml) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Greylists can't contain any negative rules, so remove the leading "!".
  if (source->contains_inverted_rules) {
    for (auto& rule : xml.rules) {
      if (base::StartsWith(rule, "!", base::CompareCase::SENSITIVE))
        rule.erase(0, 1);
    }
  }

  if (xml.error)
    LOG(ERROR) << *xml.error;
  std::move(source->parsed_callback).Run(std::move(xml));

  // Run the "all done" callback if this was the last ruleset.
  counter_++;
  DCHECK(counter_ <= sources_.size());
  if (counter_ == sources_.size())
    std::move(all_done_callback_).Run();
}

BrowserSwitcherService::BrowserSwitcherService(Profile* profile)
    : prefs_(profile),
      driver_(new AlternativeBrowserDriverImpl(&prefs_)),
      sitelist_(new BrowserSwitcherSitelistImpl(&prefs_)),
      weak_ptr_factory_(this) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&BrowserSwitcherService::StartDownload,
                     weak_ptr_factory_.GetWeakPtr(), base::Unretained(profile)),
      fetch_delay_);
}

BrowserSwitcherService::~BrowserSwitcherService() = default;

void BrowserSwitcherService::StartDownload(Profile* profile) {
  auto sources = GetRulesetSources();
  if (!sources.empty()) {
    sitelist_downloader_ = std::make_unique<XmlDownloader>(
        profile, std::move(sources),
        base::BindOnce(&BrowserSwitcherService::OnAllRulesetsParsed,
                       base::Unretained(this)));
  }
  // Refresh in 30 minutes, so the sitelists are never too stale.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&BrowserSwitcherService::StartDownload,
                     weak_ptr_factory_.GetWeakPtr(), base::Unretained(profile)),
      refresh_delay_);
}

void BrowserSwitcherService::Shutdown() {
  prefs_.Shutdown();
}

AlternativeBrowserDriver* BrowserSwitcherService::driver() {
  return driver_.get();
}

BrowserSwitcherSitelist* BrowserSwitcherService::sitelist() {
  return sitelist_.get();
}

BrowserSwitcherPrefs& BrowserSwitcherService::prefs() {
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

std::vector<RulesetSource> BrowserSwitcherService::GetRulesetSources() {
  std::vector<RulesetSource> sources;

  GURL sitelist_url = prefs_.GetExternalSitelistUrl();
  if (sitelist_url.is_valid()) {
    sources.emplace_back(
        sitelist_url, /* invert_rules */ false,
        base::BindOnce(&BrowserSwitcherService::OnExternalSitelistParsed,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  GURL greylist_url = prefs_.GetExternalGreylistUrl();
  if (greylist_url.is_valid()) {
    sources.emplace_back(
        greylist_url, /* invert_rules */ true,
        base::BindOnce(&BrowserSwitcherService::OnExternalGreylistParsed,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  return sources;
}

void BrowserSwitcherService::OnAllRulesetsParsed() {
  sitelist_downloader_.reset();
}

void BrowserSwitcherService::OnExternalSitelistParsed(ParsedXml xml) {
  if (xml.error) {
    SYSLOG(INFO) << "Unable to parse IEEM SiteList: " << *xml.error;
  } else {
    VLOG(2) << "Done parsing external SiteList for sitelist rules. "
            << "Applying rules to future navigations.";
    sitelist()->SetExternalSitelist(std::move(xml));
  }
}

void BrowserSwitcherService::OnExternalGreylistParsed(ParsedXml xml) {
  if (xml.error) {
    SYSLOG(INFO) << "Unable to parse IEEM SiteList: " << *xml.error;
  } else {
    VLOG(2) << "Done parsing external SiteList for greylist rules. "
            << "Applying rules to future navigations.";
    sitelist()->SetExternalGreylist(std::move(xml));
  }
}

base::TimeDelta BrowserSwitcherService::fetch_delay_ = kFetchSitelistDelay;
base::TimeDelta BrowserSwitcherService::refresh_delay_ = kRefreshSitelistDelay;

// static
void BrowserSwitcherService::SetFetchDelayForTesting(base::TimeDelta delay) {
  fetch_delay_ = delay;
}

void BrowserSwitcherService::SetRefreshDelayForTesting(base::TimeDelta delay) {
  refresh_delay_ = delay;
}

}  // namespace browser_switcher
