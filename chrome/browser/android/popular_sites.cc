// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/popular_sites.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/file_downloader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/google/core/browser/google_util.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

const char kPopularSitesURLFormat[] = "https://www.gstatic.com/chrome/ntp/%s";
const char kPopularSitesServerFilenameFormat[] = "suggested_sites_%s_%s.json";
const char kPopularSitesDefaultCountryCode[] = "DEFAULT";
const char kPopularSitesDefaultVersion[] = "3";
const char kPopularSitesLocalFilename[] = "suggested_sites.json";

// Extract the country from the default search engine if the default search
// engine is Google.
std::string GetDefaultSearchEngineCountryCode(Profile* profile) {
  DCHECK(profile);

  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(switches::kEnableNTPSearchEngineCountryDetection))
    return std::string();

  const TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  DCHECK(template_url_service);

  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  // It's possible to not have a default provider in the case that the default
  // search engine is defined by policy.
  if (default_provider) {
    bool is_google_search_engine =
        TemplateURLPrepopulateData::GetEngineType(
            *default_provider, template_url_service->search_terms_data()) ==
        SearchEngineType::SEARCH_ENGINE_GOOGLE;

    if (is_google_search_engine) {
      GURL search_url = default_provider->GenerateSearchURL(
          template_url_service->search_terms_data());
      return google_util::GetGoogleCountryCode(search_url);
    }
  }

  return std::string();
}

// Get the country that the experiment is running under
std::string GetVariationsServiceCountry() {
  DCHECK(g_browser_process);
  variations::VariationsService* variations_service =
      g_browser_process->variations_service();
  if (variations_service)
    return variations_service->GetStoredPermanentCountry();
  return std::string();
}

// Find out the country code of the user by using the Google country code if
// Google is the default search engine set. If Google is not the default search
// engine use the country provided by VariationsService. Fallback to a default
// if we can't make an educated guess.
std::string GetCountryCode(Profile* profile) {
  std::string country_code = GetDefaultSearchEngineCountryCode(profile);

  if (country_code.empty())
    country_code = GetVariationsServiceCountry();

  if (country_code.empty())
    country_code = kPopularSitesDefaultCountryCode;

  return base::ToUpperASCII(country_code);
}

std::string GetPopularSitesServerFilename(
    Profile* profile,
    const std::string& override_country,
    const std::string& override_version,
    const std::string& override_filename) {
  if (!override_filename.empty())
    return override_filename;

  std::string country =
      !override_country.empty() ? override_country : GetCountryCode(profile);
  std::string version = !override_version.empty() ? override_version
                                                  : kPopularSitesDefaultVersion;
  return base::StringPrintf(kPopularSitesServerFilenameFormat,
                            country.c_str(), version.c_str());
}

GURL GetPopularSitesURL(Profile* profile,
                        const std::string& override_country,
                        const std::string& override_version,
                        const std::string& override_filename) {
  return GURL(base::StringPrintf(
      kPopularSitesURLFormat,
      GetPopularSitesServerFilename(profile, override_country, override_version,
                                    override_filename)
          .c_str()));
}

GURL GetPopularSitesFallbackURL(Profile* profile) {
  return GetPopularSitesURL(profile, kPopularSitesDefaultCountryCode,
                            kPopularSitesDefaultVersion, std::string());
}

base::FilePath GetPopularSitesPath() {
  base::FilePath dir;
  PathService::Get(chrome::DIR_USER_DATA, &dir);
  return dir.AppendASCII(kPopularSitesLocalFilename);
}

scoped_ptr<std::vector<PopularSites::Site>> ReadAndParseJsonFile(
    const base::FilePath& path) {
  std::string json;
  if (!base::ReadFileToString(path, &json)) {
    DLOG(WARNING) << "Failed reading file";
    return nullptr;
  }

  scoped_ptr<base::Value> value =
      base::JSONReader::Read(json, base::JSON_ALLOW_TRAILING_COMMAS);
  base::ListValue* list;
  if (!value || !value->GetAsList(&list)) {
    DLOG(WARNING) << "Failed parsing json";
    return nullptr;
  }

  scoped_ptr<std::vector<PopularSites::Site>> sites(
      new std::vector<PopularSites::Site>);
  for (size_t i = 0; i < list->GetSize(); i++) {
    base::DictionaryValue* item;
    if (!list->GetDictionary(i, &item))
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

    sites->push_back(PopularSites::Site(title, GURL(url), GURL(favicon_url),
                                        GURL(large_icon_url),
                                        GURL(thumbnail_url)));
  }

  return sites.Pass();
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
      thumbnail_url(thumbnail_url) {}

PopularSites::Site::~Site() {}

PopularSites::PopularSites(Profile* profile,
                           const std::string& override_country,
                           const std::string& override_version,
                           const std::string& override_filename,
                           bool force_download,
                           const FinishedCallback& callback)
    : callback_(callback),
      popular_sites_local_path_(GetPopularSitesPath()),
      weak_ptr_factory_(this) {
  // Re-download the file once on every Chrome startup, but use the cached
  // local file afterwards.
  static bool first_time = true;
  FetchPopularSites(GetPopularSitesURL(profile, override_country,
                                       override_version, override_filename),
                    profile, first_time || force_download);
  first_time = false;
}

PopularSites::PopularSites(Profile* profile,
                           const GURL& url,
                           const FinishedCallback& callback)
    : callback_(callback),
      popular_sites_local_path_(GetPopularSitesPath()),
      weak_ptr_factory_(this) {
  FetchPopularSites(url, profile, true);
}

PopularSites::~PopularSites() {}

void PopularSites::FetchPopularSites(const GURL& url,
                                     Profile* profile,
                                     bool force_download) {
  downloader_.reset(
      new FileDownloader(url, popular_sites_local_path_, force_download,
                         profile->GetRequestContext(),
                         base::Bind(&PopularSites::OnDownloadDone,
                                    base::Unretained(this), profile)));
}

void PopularSites::FetchFallbackSites(Profile* profile) {
  downloader_.reset(new FileDownloader(
      GetPopularSitesFallbackURL(profile), popular_sites_local_path_,
      true /* force_download */, profile->GetRequestContext(),
      base::Bind(&PopularSites::OnDownloadFallbackDone,
                 base::Unretained(this))));
}

void PopularSites::OnDownloadDone(Profile* profile, bool success) {
  if (success) {
    ParseSiteList(popular_sites_local_path_);
    downloader_.reset();
  } else {
    DLOG(WARNING) << "Download country site list failed";
    FetchFallbackSites(profile);
  }
}

void PopularSites::OnDownloadFallbackDone(bool success) {
  if (success) {
    ParseSiteList(popular_sites_local_path_);
  } else {
    DLOG(WARNING) << "Download fallback site list failed";
    callback_.Run(false);
  }
  downloader_.reset();
}

void PopularSites::ParseSiteList(const base::FilePath& path) {
  base::PostTaskAndReplyWithResult(
      BrowserThread::GetBlockingPool()
          ->GetTaskRunnerWithShutdownBehavior(
              base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN)
          .get(),
      FROM_HERE, base::Bind(&ReadAndParseJsonFile, path),
      base::Bind(&PopularSites::OnJsonParsed, weak_ptr_factory_.GetWeakPtr()));
}

void PopularSites::OnJsonParsed(scoped_ptr<std::vector<Site>> sites) {
  if (sites)
    sites_.swap(*sites);
  else
    sites_.clear();
  callback_.Run(!!sites);
}
