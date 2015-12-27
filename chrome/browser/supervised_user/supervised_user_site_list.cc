// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_site_list.h"

#include <algorithm>

#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
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
  return value;
}

}  // namespace

SupervisedUserSiteList::HostnameHash::HostnameHash(
    const std::string& hostname) {
  base::SHA1HashBytes(reinterpret_cast<const unsigned char*>(hostname.c_str()),
                      hostname.size(), bytes_.data());
}

SupervisedUserSiteList::HostnameHash::HostnameHash(
    const std::vector<uint8_t>& bytes) {
  CHECK_GE(bytes.size(), base::kSHA1Length);
  std::copy(bytes.begin(), bytes.end(), bytes_.begin());
}

bool SupervisedUserSiteList::HostnameHash::operator==(
    const HostnameHash& rhs) const {
  return bytes_ == rhs.bytes_;
}

size_t SupervisedUserSiteList::HostnameHash::hash() const {
  // This just returns the first sizeof(size_t) bytes of |bytes_|.
  return *reinterpret_cast<const size_t*>(bytes_.data());
}

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
      // |hash_str| should be a hex-encoded SHA1 hash string.
      std::string hash_str;
      std::vector<uint8_t> hash_bytes;
      if (!entry->GetAsString(&hash_str) ||
          hash_str.size() != 2 * base::kSHA1Length ||
          !base::HexStringToBytes(hash_str, &hash_bytes)) {
        LOG(ERROR) << "Invalid hostname_hashes entry";
        continue;
      }
      DCHECK_EQ(base::kSHA1Length, hash_bytes.size());
      hostname_hashes_.push_back(HostnameHash(hash_bytes));
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
