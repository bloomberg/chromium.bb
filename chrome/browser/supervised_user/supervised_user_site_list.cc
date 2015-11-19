// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_site_list.h"

#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/sha1.h"
#include "base/task_runner_util.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

const int kSitelistFormatVersion = 2;

const char kEntryPointUrlKey[] = "entry_point_url";
const char kHostnameHashesKey[] = "hostname_hashes";
const char kSitelistFormatVersionKey[] = "version";
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

}  // namespace

void SupervisedUserSiteList::Load(const base::string16& title,
                                  const base::FilePath& path,
                                  const LoadedCallback& callback) {
  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool()
          ->GetTaskRunnerWithShutdownBehavior(
              base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN)
          .get(),
      FROM_HERE, base::Bind(&ReadFileOnBlockingThread, path),
      base::Bind(&SupervisedUserSiteList::OnJsonLoaded, title, path,
                 base::TimeTicks::Now(), callback));
}

SupervisedUserSiteList::SupervisedUserSiteList(
    const base::string16& title,
    const GURL& entry_point,
    const base::ListValue* patterns,
    const base::ListValue* hostname_hashes)
    : title_(title), entry_point_(entry_point) {
  if (patterns) {
    for (const base::Value* entry : *patterns) {
      std::string pattern;
      if (!entry->GetAsString(&pattern)) {
        LOG(ERROR) << "Invalid whitelist entry";
        continue;
      }

      patterns_.push_back(pattern);
    }
  }

  if (hostname_hashes) {
    for (const base::Value* entry : *hostname_hashes) {
      // |hash| should be a hex-encoded SHA1 hash.
      std::string hash;
      if (!entry->GetAsString(&hash) || hash.size() != 2 * base::kSHA1Length) {
        LOG(ERROR) << "Invalid hostname_hashes entry";
        continue;
      }
      // TODO(treib): Check that |hash| has only characters from [0-9a-fA-F].
      // Or just store the raw bytes (from base::HexStringToBytes).
      hostname_hashes_.push_back(hash);
    }
  }

  if (patterns_.empty() && hostname_hashes_.empty())
    LOG(WARNING) << "Site list is empty!";
}

SupervisedUserSiteList::~SupervisedUserSiteList() {
}

// static
void SupervisedUserSiteList::OnJsonLoaded(
    const base::string16& title,
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
  if (version != kSitelistFormatVersion) {
    LOG(ERROR) << "Site list " << path.value() << " has wrong version "
               << version << ", expected " << kSitelistFormatVersion;
    return;
  }

  std::string entry_point_url;
  dict->GetString(kEntryPointUrlKey, &entry_point_url);

  base::ListValue* patterns = nullptr;
  dict->GetList(kWhitelistKey, &patterns);

  base::ListValue* hostname_hashes = nullptr;
  dict->GetList(kHostnameHashesKey, &hostname_hashes);

  callback.Run(make_scoped_refptr(new SupervisedUserSiteList(
      title, GURL(entry_point_url), patterns, hostname_hashes)));
}
