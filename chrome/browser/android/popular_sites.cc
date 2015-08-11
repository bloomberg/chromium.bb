// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/popular_sites.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/task_runner_util.h"
#include "base/values.h"
#include "chrome/browser/net/file_downloader.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

const char kPopularSitesFilename[] = "ntp-popular-sites.json";
const char kPopularSitesURL[] =
    "https://www.gstatic.com/chrome/ntp/suggested_sites_IN_1.json";

base::FilePath GetPopularSitesPath() {
  base::FilePath dir;
  PathService::Get(chrome::DIR_USER_DATA, &dir);
  return dir.AppendASCII(kPopularSitesFilename);
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
    sites->push_back(PopularSites::Site(title, GURL(url), GURL(favicon_url)));
  }

  return sites.Pass();
}

}  // namespace

PopularSites::Site::Site(const base::string16& title,
                         const GURL& url,
                         const GURL& favicon_url)
    : title(title), url(url), favicon_url(favicon_url) {}

PopularSites::PopularSites(net::URLRequestContextGetter* request_context,
                           const FinishedCallback& callback)
    : callback_(callback), weak_ptr_factory_(this) {
  base::FilePath path = GetPopularSitesPath();
  downloader_.reset(new FileDownloader(
      GURL(kPopularSitesURL), path, request_context,
      base::Bind(&PopularSites::OnDownloadDone, base::Unretained(this), path)));
}

PopularSites::~PopularSites() {}

void PopularSites::OnDownloadDone(const base::FilePath& path, bool success) {
  if (success) {
    base::PostTaskAndReplyWithResult(
        BrowserThread::GetBlockingPool()->GetTaskRunnerWithShutdownBehavior(
            base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN).get(),
        FROM_HERE,
        base::Bind(&ReadAndParseJsonFile, path),
        base::Bind(&PopularSites::OnJsonParsed,
                   weak_ptr_factory_.GetWeakPtr()));
  } else {
    DLOG(WARNING) << "Download failed";
    callback_.Run(false);
  }

  downloader_.reset();
}

void PopularSites::OnJsonParsed(scoped_ptr<std::vector<Site>> sites) {
  if (sites)
    sites_.swap(*sites);
  else
    sites_.clear();
  callback_.Run(!!sites);
}
