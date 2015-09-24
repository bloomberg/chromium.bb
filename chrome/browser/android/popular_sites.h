// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_POPULAR_SITES_H_
#define CHROME_BROWSER_ANDROID_POPULAR_SITES_H_

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}

namespace net {
class URLRequestContextGetter;
}

class FileDownloader;
class Profile;

// Downloads and provides a list of suggested popular sites, for display on
// the NTP when there are not enough personalized suggestions. Caches the
// downloaded file on disk to avoid re-downloading on every startup.
class PopularSites {
 public:
  struct Site {
    Site(const base::string16& title,
         const GURL& url,
         const GURL& favicon_url,
         const GURL& thumbnail_url);
    ~Site();

    base::string16 title;
    GURL url;
    GURL favicon_url;
    GURL thumbnail_url;
  };

  using FinishedCallback = base::Callback<void(bool /* success */)>;

  // Usually, the name of the file that's downloaded is based on the user's
  // locale. |override_country| (if non-empty) is used to override the
  // auto-detected country. |override_version|, if non-empty, will
  // override the baked-in default version.
  // |override_filename|, if non-empty, will override the full filename
  // (so |override_country| and |override_version| are ignored in this case).
  // Set |force_download| to enfore re-downloading the suggestions file, even if
  // it already exists on disk.
  // TODO(treib): Get rid of |override_filename|.
  PopularSites(Profile* profile,
               const std::string& override_country,
               const std::string& override_version,
               const std::string& override_filename,
               bool force_download,
               const FinishedCallback& callback);

  // This fetches the popular sites from a given url and is only used for
  // debugging through the popular-sites-internals page.
  PopularSites(Profile* profile,
               const GURL& url,
               const FinishedCallback& callback);
  ~PopularSites();

  const std::vector<Site>& sites() const { return sites_; }

 private:
  void FetchPopularSites(const GURL& url,
                         net::URLRequestContextGetter* request_context,
                         bool force_download);
  void OnDownloadDone(const base::FilePath& path, bool success);
  void OnJsonParsed(scoped_ptr<std::vector<Site>> sites);

  FinishedCallback callback_;
  scoped_ptr<FileDownloader> downloader_;
  std::vector<Site> sites_;

  base::WeakPtrFactory<PopularSites> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PopularSites);
};

#endif  // CHROME_BROWSER_ANDROID_POPULAR_SITES_H_
