// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREF_HASH_CALCULATOR_H_
#define CHROME_BROWSER_PREFS_PREF_HASH_CALCULATOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class Value;
}  // namespace base

// Calculates and validates preference value hashes.
class PrefHashCalculator {
 public:
  enum ValidationResult {
    INVALID,
    VALID,
    // Valid under a deprecated but as secure algorithm.
    VALID_SECURE_LEGACY,
    // Valid under a deprecated and less secure algorithm.
    VALID_WEAK_LEGACY,
  };

  typedef base::Callback<std::string(const std::string& modern_device_id)>
      GetLegacyDeviceIdCallback;

  // Constructs a PrefHashCalculator using |seed| and |device_id|. The same
  // parameters must be used in order to successfully validate generated hashes.
  // |device_id| may be empty.
  PrefHashCalculator(const std::string& seed, const std::string& device_id);

  // Same as the constructor above, but also specifies that
  // |get_legacy_device_id_callback| should be used rather than the default to
  // obtain the legacy device id if required.
  PrefHashCalculator(
      const std::string& seed,
      const std::string& device_id,
      const GetLegacyDeviceIdCallback& get_legacy_device_id_callback);

  ~PrefHashCalculator();

  // Calculates a hash value for the supplied preference |path| and |value|.
  // |value| may be null if the preference has no value.
  std::string Calculate(const std::string& path, const base::Value* value)
      const;

  // Validates the provided preference hash using current and legacy hashing
  // algorithms.
  ValidationResult Validate(const std::string& path,
                            const base::Value* value,
                            const std::string& hash) const;

 private:
  // Returns the legacy device id based off of |raw_device_id_|. This method
  // lazily gets the legacy device id via |get_legacy_device_id_callback_| and
  // caches the result in |legacy_device_id_instance_| for future retrievals.
  std::string RetrieveLegacyDeviceId() const;

  const std::string seed_;
  const std::string device_id_;

  // The raw device id from which the legacy device id will be derived if
  // required.
  const std::string raw_device_id_;

  const GetLegacyDeviceIdCallback get_legacy_device_id_callback_;

  // A cache for the legacy device id which is hard to compute and thus lazily
  // computed when/if required (computing the original value for this instance
  // is allowed in const methods).
  mutable scoped_ptr<const std::string> legacy_device_id_instance_;

  DISALLOW_COPY_AND_ASSIGN(PrefHashCalculator);
};

#endif  // CHROME_BROWSER_PREFS_PREF_HASH_CALCULATOR_H_
