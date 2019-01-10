// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BADGING_BADGE_SERVICE_DELEGATE_H_
#define CHROME_BROWSER_BADGING_BADGE_SERVICE_DELEGATE_H_

#include "base/callback.h"
#include "base/optional.h"

namespace content {
class WebContents;
}

// Delegate for setting and clearing app badges. This is implemented at a
// UI-layer, allowing for a platform specific implementation of badging.
class BadgeServiceDelegate {
 public:
  using SetBadgeCallback =
      base::RepeatingCallback<void(content::WebContents*,
                                   base::Optional<uint64_t>)>;
  using ClearBadgeCallback =
      base::RepeatingCallback<void(content::WebContents*)>;

  BadgeServiceDelegate();
  ~BadgeServiceDelegate();

  // Sets the Badge for |web_contents|.
  void SetBadge(content::WebContents* web_contents,
                base::Optional<uint64_t> badge_content);

  // Clears the badge for |web_contents|.
  void ClearBadge(content::WebContents* web_contents);

  // Swaps out the implementation of |SetBadge| and |ClearBadge| for testing.
  void SetImplForTesting(SetBadgeCallback on_set_badge,
                         ClearBadgeCallback on_clear_badge);

 private:
  SetBadgeCallback on_set_badge_;
  ClearBadgeCallback on_clear_badge_;
};

#endif  // CHROME_BROWSER_BADGING_BADGE_SERVICE_DELEGATE_H_
