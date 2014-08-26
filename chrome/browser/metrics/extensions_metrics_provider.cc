// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/extensions_metrics_provider.h"

#include <algorithm>
#include <set>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/proto/system_profile.pb.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
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

// Possible states for extensions. The order of these enum values is important,
// and is used when combining the state of multiple extensions and multiple
// profiles. Combining two states should always result in the higher state.
// Ex: One profile is in state FROM_STORE_VERIFIED, and another is in
// FROM_STORE_UNVERIFIED. The state of the two profiles together will be
// FROM_STORE_UNVERIFIED.
// This enum should be kept in sync with the corresponding enum in
// components/metrics/proto/system_profile.proto
enum ExtensionState {
  NO_EXTENSIONS,
  FROM_STORE_VERIFIED,
  FROM_STORE_UNVERIFIED,
  OFF_STORE
};

metrics::SystemProfileProto::ExtensionsState ExtensionStateAsProto(
    ExtensionState value) {
  switch (value) {
    case NO_EXTENSIONS:
      return metrics::SystemProfileProto::NO_EXTENSIONS;
    case FROM_STORE_VERIFIED:
      return metrics::SystemProfileProto::NO_OFFSTORE_VERIFIED;
    case FROM_STORE_UNVERIFIED:
      return metrics::SystemProfileProto::NO_OFFSTORE_UNVERIFIED;
    case OFF_STORE:
      return metrics::SystemProfileProto::HAS_OFFSTORE;
  }
  NOTREACHED();
  return metrics::SystemProfileProto::NO_EXTENSIONS;
}

// Determines if the |extension| is an extension (can use extension APIs) and is
// not from the webstore. If local information claims the extension is from the
// webstore, we attempt to verify with |verifier| by checking if it has been
// explicitly deemed invalid. If |verifier| is inactive or if the extension is
// unknown to |verifier|, the local information is trusted.
ExtensionState IsOffStoreExtension(
    const extensions::Extension& extension,
    const extensions::InstallVerifier& verifier) {
  if (!extension.is_extension() && !extension.is_legacy_packaged_app())
    return NO_EXTENSIONS;

  // Component extensions are considered safe.
  if (extensions::Manifest::IsComponentLocation(extension.location()))
    return NO_EXTENSIONS;

  if (verifier.AllowedByEnterprisePolicy(extension.id()))
    return NO_EXTENSIONS;

  if (!extensions::InstallVerifier::IsFromStore(extension))
    return OFF_STORE;

  // Local information about the extension implies it is from the store. We try
  // to use the install verifier to verify this.
  if (!verifier.IsKnownId(extension.id()))
    return FROM_STORE_UNVERIFIED;

  if (verifier.IsInvalid(extension.id()))
    return OFF_STORE;

  return FROM_STORE_VERIFIED;
}

// Finds the ExtensionState of |extensions|. The return value will be the
// highest (as defined by the order of ExtensionState) value of each extension
// in |extensions|.
ExtensionState CheckForOffStore(const extensions::ExtensionSet& extensions,
                                const extensions::InstallVerifier& verifier) {
  ExtensionState state = NO_EXTENSIONS;
  for (extensions::ExtensionSet::const_iterator it = extensions.begin();
       it != extensions.end() && state < OFF_STORE;
       ++it) {
    // Combine the state of each extension, always favoring the higher state as
    // defined by the order of ExtensionState.
    state = std::max(state, IsOffStoreExtension(**it, verifier));
  }
  return state;
}

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
ExtensionsMetricsProvider::GetInstalledExtensions(Profile* profile) {
  if (profile) {
    return extensions::ExtensionRegistry::Get(profile)
        ->GenerateInstalledExtensionsSet();
  }
  return scoped_ptr<extensions::ExtensionSet>();
}

uint64 ExtensionsMetricsProvider::GetClientID() {
  // TODO(blundell): Create a MetricsLog::ClientIDAsInt() API and call it
  // here as well as in MetricsLog's population of the client_id field of
  // the uma_proto.
  return metrics::MetricsLog::Hash(metrics_state_manager_->client_id());
}

void ExtensionsMetricsProvider::ProvideSystemProfileMetrics(
    metrics::SystemProfileProto* system_profile) {
  ProvideOffStoreMetric(system_profile);
  ProvideOccupiedBucketMetric(system_profile);
}

void ExtensionsMetricsProvider::ProvideOffStoreMetric(
    metrics::SystemProfileProto* system_profile) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return;

  ExtensionState state = NO_EXTENSIONS;

  // The off-store metric includes information from all loaded profiles at the
  // time when this metric is generated.
  std::vector<Profile*> profiles = profile_manager->GetLoadedProfiles();
  for (size_t i = 0u; i < profiles.size() && state < OFF_STORE; ++i) {
    extensions::InstallVerifier* verifier =
        extensions::ExtensionSystem::Get(profiles[i])->install_verifier();

    scoped_ptr<extensions::ExtensionSet> extensions(
        GetInstalledExtensions(profiles[i]));
    if (!extensions)
      continue;

    // Combine the state from each profile, always favoring the higher state as
    // defined by the order of ExtensionState.
    state = std::max(state, CheckForOffStore(*extensions.get(), *verifier));
  }

  system_profile->set_offstore_extensions_state(ExtensionStateAsProto(state));
}

void ExtensionsMetricsProvider::ProvideOccupiedBucketMetric(
    metrics::SystemProfileProto* system_profile) {
  // UMA reports do not support multiple profiles, but extensions are installed
  // per-profile.  We return the extensions installed in the primary profile.
  // In the future, we might consider reporting data about extensions in all
  // profiles.
  Profile* profile = GetMetricsProfile();

  scoped_ptr<extensions::ExtensionSet> extensions(
      GetInstalledExtensions(profile));
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
