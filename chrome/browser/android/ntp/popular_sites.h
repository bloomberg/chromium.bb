// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_NTP_POPULAR_SITES_H_
#define CHROME_BROWSER_ANDROID_NTP_POPULAR_SITES_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/net/file_downloader.h"
#include "url/gurl.h"

namespace net {
class URLRequestContextGetter;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class PrefService;
class TemplateURLService;

// Downloads and provides a list of suggested popular sites, for display on
// the NTP when there are not enough personalized suggestions. Caches the
// downloaded file on disk to avoid re-downloading on every startup.
class PopularSites {
 public:
  struct Site {
    Site(const base::string16& title,
         const GURL& url,
         const GURL& favicon_url,
         const GURL& large_icon_url,
         const GURL& thumbnail_url);
    Site(const Site& other);
    ~Site();

    base::string16 title;
    GURL url;
    GURL favicon_url;
    GURL large_icon_url;
    GURL thumbnail_url;
  };

  using FinishedCallback = base::Callback<void(bool /* success */)>;

  // Usually, the name of the file that's downloaded is based on the user's
  // locale. |override_country| (if non-empty) is used to override the
  // auto-detected country. |override_version|, if non-empty, will
  // override the baked-in default version.
  // Set |force_download| to enforce re-downloading the suggestions file, even
  // if it already exists on disk.
  PopularSites(PrefService* prefs,
               const TemplateURLService* template_url_service,
               net::URLRequestContextGetter* download_context,
               const std::string& override_country,
               const std::string& override_version,
               bool force_download,
               const FinishedCallback& callback);

  // This fetches the popular sites from a given url and is only used for
  // debugging through the popular-sites-internals page.
  PopularSites(PrefService* prefs,
               const TemplateURLService* template_url_service,
               net::URLRequestContextGetter* download_context,
               const GURL& url,
               const FinishedCallback& callback);

  ~PopularSites();

  const std::vector<Site>& sites() const { return sites_; }

  // The country/version of the file that was last downloaded.
  std::string GetCountry() const;
  std::string GetVersion() const;

  const base::FilePath& local_path() const { return local_path_; }

  // Register preferences used by this class.
  static void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* user_prefs);

 private:
  PopularSites(PrefService* prefs,
               const TemplateURLService* template_url_service,
               net::URLRequestContextGetter* download_context,
               const std::string& country,
               const std::string& version,
               const GURL& override_url,
               bool force_download,
               const FinishedCallback& callback);

  GURL GetPopularSitesURL() const;

  // Fetch the popular sites at the given URL. |force_download| should be true
  // if any previously downloaded site list should be overwritten.
  void FetchPopularSites(const GURL& url,
                         bool force_download,
                         bool is_fallback);

  // If the download was not successful and it was not a fallback, attempt to
  // download the fallback suggestions.
  void OnDownloadDone(bool is_fallback, FileDownloader::Result result);

  void ParseSiteList(const base::FilePath& path);
  void OnJsonParsed(std::unique_ptr<std::vector<Site>> sites);

  FinishedCallback callback_;
  std::unique_ptr<FileDownloader> downloader_;
  std::vector<Site> sites_;
  std::string pending_country_;
  std::string pending_version_;

  base::FilePath local_path_;

  PrefService* prefs_;
  const TemplateURLService* template_url_service_;
  net::URLRequestContextGetter* download_context_;

  base::WeakPtrFactory<PopularSites> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PopularSites);
};

#endif  // CHROME_BROWSER_ANDROID_NTP_POPULAR_SITES_H_
