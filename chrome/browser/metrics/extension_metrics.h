// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_EXTENSION_METRICS_H_
#define CHROME_BROWSER_METRICS_EXTENSION_METRICS_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

class Profile;

namespace extensions {
class ExtensionSet;
}

namespace metrics {
class SystemProfileProto;
}

// HashedExtensionMetrics groups various constants and functions used for
// reporting extension IDs with UMA reports (after hashing the extension IDs
// for privacy).
class HashedExtensionMetrics {
 public:
  explicit HashedExtensionMetrics(uint64 client_id);
  virtual ~HashedExtensionMetrics();

  // Writes the hashed list of installed extensions into the specified
  // SystemProfileProto object.
  void WriteExtensionList(metrics::SystemProfileProto* system_profile);

 protected:
  // Retrieves the set of extensions installed in the current profile.  The
  // default implementation should be fine, but this can be overridden for
  // testing.
  //
  // TODO(mvrable): If metrics are ever converted to being per-profile, then
  // this should be updated to return extensions installed in a specified
  // profile.
  virtual scoped_ptr<extensions::ExtensionSet> GetInstalledExtensions();

  // Hashes the extension extension ID using the provided client key (which
  // must be less than kExtensionListClientKeys) and to produce an output value
  // between 0 and kExtensionListBuckets-1.
  static int HashExtension(const std::string& extension_id, uint32 client_key);

 private:
  // Returns the profile for which extensions will be gathered.  Once a
  // suitable profile has been found, future calls will continue to return the
  // same value so that reported extensions are consistent.
  Profile* GetMetricsProfile();

  // The key used when hashing extension identifiers, derived from client_id.
  const int client_key_;

  // The profile for which extensions are gathered.  Once a profile is found
  // its value is cached here so that GetMetricsProfile() can return a
  // consistent value.
  Profile* cached_profile_;

  DISALLOW_COPY_AND_ASSIGN(HashedExtensionMetrics);
};

#endif  // CHROME_BROWSER_METRICS_EXTENSION_METRICS_H_
