// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/extensions_metrics_provider.h"

#include <set>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/proto/system_profile.pb.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_set.h"
#include "third_party/smhasher/src/City.h"

namespace {

// The number of possible hash keys that a client may use.  The UMA client_id
// value is reduced modulo this value to produce the key used by that
// particular client.
const size_t kExtensionListClientKeys = 4096;

// The number of hash buckets into which extension IDs are mapped.  This sets
// the possible output range of the HashExtension function.
const size_t kExtensionListBuckets = 1024;

}  // namespace

ExtensionsMetricsProvider::ExtensionsMetricsProvider(
    metrics::MetricsStateManager* metrics_state_manager)
    : metrics_state_manager_(metrics_state_manager), cached_profile_(NULL) {
  DCHECK(metrics_state_manager_);
}

ExtensionsMetricsProvider::~ExtensionsMetricsProvider() {
}

// static
int ExtensionsMetricsProvider::HashExtension(const std::string& extension_id,
                                             uint32 client_key) {
  DCHECK_LE(client_key, kExtensionListClientKeys);
  std::string message =
      base::StringPrintf("%u:%s", client_key, extension_id.c_str());
  uint64 output = CityHash64(message.data(), message.size());
  return output % kExtensionListBuckets;
}

Profile* ExtensionsMetricsProvider::GetMetricsProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return NULL;

  // If there is a cached profile, reuse that.  However, check that it is still
  // valid first.
  if (cached_profile_ && profile_manager->IsValidProfile(cached_profile_))
    return cached_profile_;

  // Find a suitable profile to use, and cache it so that we continue to report
  // statistics on the same profile.  We would simply use
  // ProfileManager::GetLastUsedProfile(), except that that has the side effect
  // of creating a profile if it does not yet exist.
  cached_profile_ = profile_manager->GetProfileByPath(
      profile_manager->GetLastUsedProfileDir(profile_manager->user_data_dir()));
  if (cached_profile_) {
    // Ensure that the returned profile is not an incognito profile.
    cached_profile_ = cached_profile_->GetOriginalProfile();
  }
  return cached_profile_;
}

scoped_ptr<extensions::ExtensionSet>
ExtensionsMetricsProvider::GetInstalledExtensions() {
#if defined(ENABLE_EXTENSIONS)
  // UMA reports do not support multiple profiles, but extensions are installed
  // per-profile.  We return the extensions installed in the primary profile.
  // In the future, we might consider reporting data about extensions in all
  // profiles.
  Profile* profile = GetMetricsProfile();
  if (profile) {
    return extensions::ExtensionRegistry::Get(profile)
        ->GenerateInstalledExtensionsSet();
  }
#endif  // defined(ENABLE_EXTENSIONS)
  return scoped_ptr<extensions::ExtensionSet>();
}

uint64 ExtensionsMetricsProvider::GetClientID() {
  // TODO(blundell): Create a MetricsLog::ClientIDAsInt() API and call it
  // here as well as in MetricsLog's population of the client_id field of
  // the uma_proto.
  return MetricsLog::Hash(metrics_state_manager_->client_id());
}

void ExtensionsMetricsProvider::ProvideSystemProfileMetrics(
    metrics::SystemProfileProto* system_profile) {
  scoped_ptr<extensions::ExtensionSet> extensions(GetInstalledExtensions());
  if (!extensions)
    return;

  const int client_key = GetClientID() % kExtensionListClientKeys;

  std::set<int> buckets;
  for (extensions::ExtensionSet::const_iterator it = extensions->begin();
       it != extensions->end();
       ++it) {
    buckets.insert(HashExtension((*it)->id(), client_key));
  }

  for (std::set<int>::const_iterator it = buckets.begin(); it != buckets.end();
       ++it) {
    system_profile->add_occupied_extension_bucket(*it);
  }
}
