// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/unexpire_flags.h"

#include "chrome/browser/expired_flags_list.h"

namespace flags {

const base::Feature kUnexpireFlagsM76{"TemporaryUnexpireFlagsM76",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

bool ExpiryEnabledForMstone(int mstone) {
  // generate_expired_list.py will never emit flags with expiry milestone -1, to
  // keep binary size down. However, if a bug *did* cause that to happen, and
  // this function did not handle that case, disaster could ensue: all the -1
  // flags that are supposed to never expire would in fact expire instantly,
  // since -1 < x for any valid mstone x.
  // As such, there's an extra error-check here: never allow flags with mstone
  // -1 to expire.
  DCHECK(mstone != -1);
  if (mstone == -1)
    return false;

  // Currently expiration targets flags expiring in M76 or earlier. In M79 this
  // will become M78 or earlier; in M80 it will become M80 or earlier, and in
  // all future milestones Mx it will be Mx or earlier, so this logic will cease
  // to hardcode a milestone and instead target the current major version.
  if (mstone < 76)
    return true;
  if (mstone == 76)
    return !base::FeatureList::IsEnabled(kUnexpireFlagsM76);
  return false;
}

bool IsFlagExpired(const char* internal_name) {
  for (int i = 0; kExpiredFlags[i].name; ++i) {
    const ExpiredFlag* f = &kExpiredFlags[i];
    if (!strcmp(f->name, internal_name) && ExpiryEnabledForMstone(f->mstone))
      return true;
  }
  return false;
}

}  // namespace flags
