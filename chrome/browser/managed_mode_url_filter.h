// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_URL_FILTER_H_
#define CHROME_BROWSER_MANAGED_MODE_URL_FILTER_H_

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/values.h"

class FilePath;
class GURL;

namespace extensions {
class URLMatcher;
}

class ManagedModeURLFilter : public base::NonThreadSafe {
 public:
  ManagedModeURLFilter();
  ~ManagedModeURLFilter();

  // Returns true if the URL is matched by the filter.
  bool IsURLWhitelisted(const GURL& url) const;

  // Sets whether the filter is active or not (default is inactive).
  // If the filter is inactive, the whitelist will match any URL.
  void SetActive(bool in_managed_mode);

  // Sets the whitelist from the passed in list of |patterns|.
  void SetWhitelist(const std::vector<std::string>& patterns,
                    const base::Closure& continuation);

 private:
  void SetURLMatcher(const base::Closure& callback,
                     scoped_ptr<extensions::URLMatcher> url_matcher);

  base::WeakPtrFactory<ManagedModeURLFilter> weak_ptr_factory_;
  bool active_;
  scoped_ptr<extensions::URLMatcher> url_matcher_;

  DISALLOW_COPY_AND_ASSIGN(ManagedModeURLFilter);
};

#endif  // CHROME_BROWSER_MANAGED_MODE_URL_FILTER_H_
