// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/popular_sites_impl.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/google/core/browser/google_util.h"
#include "components/ntp_tiles/constants.h"
#include "components/ntp_tiles/field_trial.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/ntp_tiles/switches.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

#if defined(OS_ANDROID) || defined(OS_IOS)
#include "base/json/json_reader.h"
#include "components/grit/components_resources.h"
#include "ui/base/resource/resource_bundle.h"
#endif

#if defined(OS_IOS)
#include "components/ntp_tiles/country_code_ios.h"
#endif

using net::URLFetcher;
using variations::VariationsService;

namespace ntp_tiles {

namespace {

const char kPopularSitesURLFormat[] =
    "https://www.gstatic.com/chrome/ntp/suggested_sites_%s_%s.json";
const char kPopularSitesDefaultCountryCode[] = "DEFAULT";
const char kPopularSitesDefaultVersion[] = "5";
const int kPopularSitesRedownloadIntervalHours = 24;

const char kPopularSitesLastDownloadPref[] = "popular_sites_last_download";
const char kPopularSitesURLPref[] = "popular_sites_url";
const char kPopularSitesJsonPref[] = "suggested_sites_json";

// TODO(crbug.com/683890): This refers to a local cache stored by older
// versions of Chrome, no longer used. Remove after M61.
const char kPopularSitesLocalFilenameToCleanup[] = "suggested_sites.json";

GURL GetPopularSitesURL(const std::string& country,
                        const std::string& version) {
  return GURL(base::StringPrintf(kPopularSitesURLFormat, country.c_str(),
                                 version.c_str()));
}

// Extract the country from the default search engine if the default search
// engine is Google.
std::string GetDefaultSearchEngineCountryCode(
    const TemplateURLService* template_url_service) {
  DCHECK(template_url_service);

  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(
          ntp_tiles::switches::kEnableNTPSearchEngineCountryDetection))
    return std::string();

  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  // It's possible to not have a default provider in the case that the default
  // search engine is defined by policy.
  if (default_provider) {
    bool is_google_search_engine =
        default_provider->GetEngineType(
            template_url_service->search_terms_data()) ==
        SearchEngineType::SEARCH_ENGINE_GOOGLE;

    if (is_google_search_engine) {
      GURL search_url = default_provider->GenerateSearchURL(
          template_url_service->search_terms_data());
      return google_util::GetGoogleCountryCode(search_url);
    }
  }

  return std::string();
}

std::string GetVariationCountry() {
  return variations::GetVariationParamValue(kPopularSitesFieldTrialName,
                                            "country");
}

std::string GetVariationVersion() {
  return variations::GetVariationParamValue(kPopularSitesFieldTrialName,
                                            "version");
}

PopularSites::SitesVector ParseSiteList(const base::ListValue& list) {
  PopularSites::SitesVector sites;
  for (size_t i = 0; i < list.GetSize(); i++) {
    const base::DictionaryValue* item;
    if (!list.GetDictionary(i, &item))
      continue;
    base::string16 title;
    std::string url;
    if (!item->GetString("title", &title) || !item->GetString("url", &url))
      continue;
    std::string favicon_url;
    item->GetString("favicon_url", &favicon_url);
    std::string thumbnail_url;
    item->GetString("thumbnail_url", &thumbnail_url);
    std::string large_icon_url;
    item->GetString("large_icon_url", &large_icon_url);

    sites.emplace_back(title, GURL(url), GURL(favicon_url),
                       GURL(large_icon_url), GURL(thumbnail_url));
    item->GetInteger("default_icon_resource",
                     &sites.back().default_icon_resource);
  }
  return sites;
}

#if defined(GOOGLE_CHROME_BUILD) && (defined(OS_ANDROID) || defined(OS_IOS))
void SetDefaultResourceForSite(int index,
                               int resource_id,
                               base::ListValue* sites) {
  base::DictionaryValue* site;
  if (!sites->GetDictionary(index, &site)) {
    return;
  }
  site->SetInteger("default_icon_resource", resource_id);
}
#endif

// Creates the list of popular sites based on a snapshot available for mobile.
std::unique_ptr<base::ListValue> DefaultPopularSites() {
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  return base::MakeUnique<base::ListValue>();
#else
  if (!base::FeatureList::IsEnabled(kPopularSitesBakedInContentFeature)) {
    return base::MakeUnique<base::ListValue>();
  }
  std::unique_ptr<base::ListValue> sites =
      base::ListValue::From(base::JSONReader::Read(
          ResourceBundle::GetSharedInstance().GetRawDataResource(
              IDR_DEFAULT_POPULAR_SITES_JSON)));
  DCHECK(sites);
#if defined(GOOGLE_CHROME_BUILD)
  int index = 0;
  for (int icon_resource :
       {IDR_DEFAULT_POPULAR_SITES_ICON0, IDR_DEFAULT_POPULAR_SITES_ICON1,
        IDR_DEFAULT_POPULAR_SITES_ICON2, IDR_DEFAULT_POPULAR_SITES_ICON3,
        IDR_DEFAULT_POPULAR_SITES_ICON4, IDR_DEFAULT_POPULAR_SITES_ICON5,
        IDR_DEFAULT_POPULAR_SITES_ICON6, IDR_DEFAULT_POPULAR_SITES_ICON7}) {
    SetDefaultResourceForSite(index++, icon_resource, sites.get());
  }
#endif  // GOOGLE_CHROME_BUILD
  return sites;
#endif  // OS_ANDROID || OS_IOS
}

}  // namespace

PopularSites::Site::Site(const base::string16& title,
                         const GURL& url,
                         const GURL& favicon_url,
                         const GURL& large_icon_url,
                         const GURL& thumbnail_url)
    : title(title),
      url(url),
      favicon_url(favicon_url),
      large_icon_url(large_icon_url),
      thumbnail_url(thumbnail_url),
      default_icon_resource(-1) {}

PopularSites::Site::Site(const Site& other) = default;

PopularSites::Site::~Site() {}

PopularSitesImpl::PopularSitesImpl(
    const scoped_refptr<base::SequencedWorkerPool>& blocking_pool,
    PrefService* prefs,
    const TemplateURLService* template_url_service,
    VariationsService* variations_service,
    net::URLRequestContextGetter* download_context,
    const base::FilePath& directory,
    ParseJSONCallback parse_json)
    : blocking_runner_(blocking_pool->GetTaskRunnerWithShutdownBehavior(
          base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN)),
      prefs_(prefs),
      template_url_service_(template_url_service),
      variations_(variations_service),
      download_context_(download_context),
      parse_json_(std::move(parse_json)),
      is_fallback_(false),
      sites_(ParseSiteList(*prefs->GetList(kPopularSitesJsonPref))),
      weak_ptr_factory_(this) {
  // If valid path provided, remove local files created by older versions.
  if (!directory.empty() && blocking_runner_) {
    blocking_runner_->PostTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(&base::DeleteFile),
                   directory.AppendASCII(kPopularSitesLocalFilenameToCleanup),
                   /*recursive=*/false));
  }
}

PopularSitesImpl::~PopularSitesImpl() {}

bool PopularSitesImpl::MaybeStartFetch(bool force_download,
                                       const FinishedCallback& callback) {
  DCHECK(!callback_);
  callback_ = callback;

  const base::Time last_download_time = base::Time::FromInternalValue(
      prefs_->GetInt64(kPopularSitesLastDownloadPref));
  const base::TimeDelta time_since_last_download =
      base::Time::Now() - last_download_time;
  const base::TimeDelta redownload_interval =
      base::TimeDelta::FromHours(kPopularSitesRedownloadIntervalHours);
  const bool download_time_is_future = base::Time::Now() < last_download_time;

  pending_url_ = GetURLToFetch();
  const bool url_changed =
      pending_url_.spec() != prefs_->GetString(kPopularSitesURLPref);

  // Download forced, or we need to download a new file.
  if (force_download || download_time_is_future ||
      (time_since_last_download > redownload_interval) || url_changed) {
    FetchPopularSites();
    return true;
  }
  return false;
}

const PopularSites::SitesVector& PopularSitesImpl::sites() const {
  return sites_;
}

GURL PopularSitesImpl::GetLastURLFetched() const {
  return GURL(prefs_->GetString(kPopularSitesURLPref));
}

GURL PopularSitesImpl::GetURLToFetch() {
  const std::string country = GetCountryToFetch();
  const std::string version = GetVersionToFetch();

  const GURL override_url =
      GURL(prefs_->GetString(ntp_tiles::prefs::kPopularSitesOverrideURL));
  return override_url.is_valid() ? override_url
                                 : GetPopularSitesURL(country, version);
}

// Determine the country code to use. In order of precedence:
// - The explicit "override country" pref set by the user.
// - The country code from the field trial config (variation parameter).
// - The Google country code if Google is the default search engine (and the
//   "--enable-ntp-search-engine-country-detection" switch is present).
// - The country provided by the VariationsService.
// - A default fallback.
std::string PopularSitesImpl::GetCountryToFetch() {
  std::string country_code =
      prefs_->GetString(ntp_tiles::prefs::kPopularSitesOverrideCountry);

  if (country_code.empty())
    country_code = GetVariationCountry();

  if (country_code.empty())
    country_code = GetDefaultSearchEngineCountryCode(template_url_service_);

  if (country_code.empty() && variations_)
    country_code = variations_->GetStoredPermanentCountry();

#if defined(OS_IOS)
  if (country_code.empty())
    country_code = GetDeviceCountryCode();
#endif

  if (country_code.empty())
    country_code = kPopularSitesDefaultCountryCode;

  return base::ToUpperASCII(country_code);
}

// Determine the version to use. In order of precedence:
// - The explicit "override version" pref set by the user.
// - The version from the field trial config (variation parameter).
// - A default fallback.
std::string PopularSitesImpl::GetVersionToFetch() {
  std::string version =
      prefs_->GetString(ntp_tiles::prefs::kPopularSitesOverrideVersion);

  if (version.empty())
    version = GetVariationVersion();

  if (version.empty())
    version = kPopularSitesDefaultVersion;

  return version;
}

const base::ListValue* PopularSitesImpl::GetCachedJson() {
  return prefs_->GetList(kPopularSitesJsonPref);
}

// static
void PopularSitesImpl::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  user_prefs->RegisterStringPref(ntp_tiles::prefs::kPopularSitesOverrideURL,
                                 std::string());
  user_prefs->RegisterStringPref(ntp_tiles::prefs::kPopularSitesOverrideCountry,
                                 std::string());
  user_prefs->RegisterStringPref(ntp_tiles::prefs::kPopularSitesOverrideVersion,
                                 std::string());

  user_prefs->RegisterInt64Pref(kPopularSitesLastDownloadPref, 0);
  user_prefs->RegisterStringPref(kPopularSitesURLPref, std::string());
  user_prefs->RegisterListPref(kPopularSitesJsonPref,
                               DefaultPopularSites().release());
}

void PopularSitesImpl::FetchPopularSites() {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("popular_sites_fetch", R"(
        semantics {
          sender: "Popular Sites New Tab Fetch"
          description:
            "Google Chrome may display a list of regionally-popular web sites "
            "on the New Tab Page. This service fetches the list of these sites."
          trigger:
            "Once per day, unless no popular web sites are required because "
            "the New Tab Page is filled with suggestions based on the user's "
            "browsing history."
          data: "A two letter country code based on the user's location."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: false
          setting: "This feature cannot be disabled in settings."
          policy_exception_justification:
            "Not implemented, considered not useful."
        })");
  fetcher_ = URLFetcher::Create(pending_url_, URLFetcher::GET, this,
                                traffic_annotation);
  data_use_measurement::DataUseUserData::AttachToFetcher(
      fetcher_.get(), data_use_measurement::DataUseUserData::NTP_TILES);
  fetcher_->SetRequestContext(download_context_);
  fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                         net::LOAD_DO_NOT_SAVE_COOKIES);
  fetcher_->SetAutomaticallyRetryOnNetworkChanges(1);
  fetcher_->Start();
}

void PopularSitesImpl::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK_EQ(fetcher_.get(), source);
  std::unique_ptr<net::URLFetcher> free_fetcher = std::move(fetcher_);

  std::string json_string;
  if (!(source->GetStatus().is_success() &&
        source->GetResponseCode() == net::HTTP_OK &&
        source->GetResponseAsString(&json_string))) {
    OnDownloadFailed();
    return;
  }

  parse_json_.Run(json_string, base::Bind(&PopularSitesImpl::OnJsonParsed,
                                          weak_ptr_factory_.GetWeakPtr()),
                  base::Bind(&PopularSitesImpl::OnJsonParseFailed,
                             weak_ptr_factory_.GetWeakPtr()));
}

void PopularSitesImpl::OnJsonParsed(std::unique_ptr<base::Value> json) {
  std::unique_ptr<base::ListValue> list =
      base::ListValue::From(std::move(json));
  if (!list) {
    DLOG(WARNING) << "JSON is not a list";
    OnDownloadFailed();
    return;
  }

  prefs_->Set(kPopularSitesJsonPref, *list);
  prefs_->SetInt64(kPopularSitesLastDownloadPref,
                   base::Time::Now().ToInternalValue());
  prefs_->SetString(kPopularSitesURLPref, pending_url_.spec());

  sites_ = ParseSiteList(*list);
  callback_.Run(true);
}

void PopularSitesImpl::OnJsonParseFailed(const std::string& error_message) {
  DLOG(WARNING) << "JSON parsing failed: " << error_message;
  OnDownloadFailed();
}

void PopularSitesImpl::OnDownloadFailed() {
  if (!is_fallback_) {
    DLOG(WARNING) << "Download country site list failed";
    is_fallback_ = true;
    pending_url_ = GetPopularSitesURL(kPopularSitesDefaultCountryCode,
                                      kPopularSitesDefaultVersion);
    FetchPopularSites();
  } else {
    DLOG(WARNING) << "Download fallback site list failed";
    callback_.Run(false);
  }
}

}  // namespace ntp_tiles
