// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTALLABLE_INSTALLABLE_PARAMS_H_
#define CHROME_BROWSER_INSTALLABLE_INSTALLABLE_PARAMS_H_

// This struct specifies the work to be done by the InstallableManager.
// Data is cached and fetched in the order specified in this struct. A web app
// manifest will always be fetched first.
struct InstallableParams {
  // The ideal primary icon size to fetch. Used only if
  // |fetch_valid_primary_icon| is true.
  int ideal_primary_icon_size_in_px = -1;

  // The minimum primary icon size to fetch. Used only if
  // |fetch_valid_primary_icon| is true.
  int minimum_primary_icon_size_in_px = -1;

  // The ideal badge icon size to fetch. Used only if
  // |fetch_valid_badge_icon| is true.
  int ideal_badge_icon_size_in_px = -1;

  // The minimum badge icon size to fetch. Used only if
  // |fetch_valid_badge_icon| is true.
  int minimum_badge_icon_size_in_px = -1;

  // Check whether there is a fetchable, non-empty icon in the manifest
  // conforming to the primary icon size parameters.
  bool fetch_valid_primary_icon = false;

  // Check whether there is a fetchable, non-empty icon in the manifest
  // conforming to the badge icon size parameters.
  bool fetch_valid_badge_icon = false;

  // Check whether the site is installable. That is, it has a manifest valid for
  // a web app and a service worker controlling the manifest start URL and the
  // current URL.
  bool check_installable = false;

  // Whether or not to wait indefinitely for a service worker. If this is set to
  // false, the worker status will not be cached and will be re-checked if
  // GetData() is called again for the current page.
  bool wait_for_worker = true;
};

#endif  // CHROME_BROWSER_INSTALLABLE_INSTALLABLE_PARAMS_H_
