// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_site_list.h"

#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task_runner_util.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

const int kSitelistFormatVersion = 1;

const char kHostnameHashesKey[] = "hostname_hashes";
const char kNameKey[] = "name";
const char kSitesKey[] = "sites";
const char kSitelistFormatVersionKey[] = "version";
const char kUrlKey[] = "url";
const char kWhitelistKey[] = "whitelist";

namespace {

scoped_ptr<base::Value> ReadFileOnBlockingThread(const base::FilePath& path) {
  SCOPED_UMA_HISTOGRAM_TIMER("ManagedUsers.Whitelist.ReadDuration");
  JSONFileValueDeserializer deserializer(path);
  int error_code;
  std::string error_msg;
  scoped_ptr<base::Value> value =
      deserializer.Deserialize(&error_code, &error_msg);
  if (!value) {
    LOG(ERROR) << "Couldn't load site list " << path.value() << ": "
               << error_msg;
  }
  return value.Pass();
}

// Takes a DictionaryValue entry from the JSON file and fills the whitelist
// (via URL patterns or hostname hashes) and the URL in the corresponding Site
// struct.
void AddWhitelistEntries(const base::DictionaryValue* site_dict,
                         SupervisedUserSiteList::Site* site) {
  bool found = false;

  const base::ListValue* whitelist = nullptr;
  if (site_dict->GetList(kWhitelistKey, &whitelist)) {
    found = true;
    for (const base::Value* entry : *whitelist) {
      std::string pattern;
      if (!entry->GetAsString(&pattern)) {
        LOG(ERROR) << "Invalid whitelist entry";
        continue;
      }

      site->patterns.push_back(pattern);
    }
  }

  const base::ListValue* hash_list = nullptr;
  if (site_dict->GetList(kHostnameHashesKey, &hash_list)) {
    found = true;
    for (const base::Value* entry : *hash_list) {
      std::string hash;
      if (!entry->GetAsString(&hash)) {
        LOG(ERROR) << "Invalid hostname_hashes entry";
        continue;
      }
      // TODO(treib): Check that |hash| has exactly 40 (2*base::kSHA1Length)
      // characters from [0-9a-fA-F]. Or just store the raw bytes (from
      // base::HexStringToBytes).

      site->hostname_hashes.push_back(hash);
    }
  }

  if (found)
    return;

  // Fall back to using a whitelist based on the URL.
  std::string url_str;
  if (!site_dict->GetString(kUrlKey, &url_str)) {
    LOG(ERROR) << "Whitelist is invalid";
    return;
  }

  GURL url(url_str);
  if (!url.is_valid()) {
    LOG(ERROR) << "URL " << url_str << " is invalid";
    return;
  }

  site->patterns.push_back(url.host());
}

}  // namespace

SupervisedUserSiteList::Site::Site(const base::string16& name) : name(name) {
}

SupervisedUserSiteList::Site::~Site() {
}

void SupervisedUserSiteList::Load(const base::FilePath& path,
                                  const LoadedCallback& callback) {
  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool()
          ->GetTaskRunnerWithShutdownBehavior(
              base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN)
          .get(),
      FROM_HERE, base::Bind(&ReadFileOnBlockingThread, path),
      base::Bind(&SupervisedUserSiteList::OnJsonLoaded, path,
                 base::TimeTicks::Now(), callback));
}

SupervisedUserSiteList::SupervisedUserSiteList(const base::ListValue& sites) {
  for (const base::Value* site : sites) {
    const base::DictionaryValue* entry = nullptr;
    if (!site->GetAsDictionary(&entry)) {
      LOG(ERROR) << "Entry is invalid";
      continue;
    }

    base::string16 name;
    entry->GetString(kNameKey, &name);
    sites_.push_back(Site(name));
    AddWhitelistEntries(entry, &sites_.back());
  }
}

SupervisedUserSiteList::~SupervisedUserSiteList() {
}

// static
void SupervisedUserSiteList::OnJsonLoaded(
    const base::FilePath& path,
    base::TimeTicks start_time,
    const SupervisedUserSiteList::LoadedCallback& callback,
    scoped_ptr<base::Value> value) {
  if (!value)
    return;

  if (!start_time.is_null()) {
    UMA_HISTOGRAM_TIMES("ManagedUsers.Whitelist.JsonParseDuration",
                        base::TimeTicks::Now() - start_time);
  }

  base::DictionaryValue* dict = nullptr;
  if (!value->GetAsDictionary(&dict)) {
    LOG(ERROR) << "Site list " << path.value() << " is invalid";
    return;
  }

  int version = 0;
  if (!dict->GetInteger(kSitelistFormatVersionKey, &version)) {
    LOG(ERROR) << "Site list " << path.value() << " has invalid version";
    return;
  }

  if (version > kSitelistFormatVersion) {
    LOG(ERROR) << "Site list " << path.value() << " has a too new format";
    return;
  }

  base::ListValue* sites = nullptr;
  if (!dict->GetList(kSitesKey, &sites)) {
    LOG(ERROR) << "Site list " << path.value() << " does not contain any sites";
    return;
  }

  callback.Run(make_scoped_refptr(new SupervisedUserSiteList(*sites)));
}
