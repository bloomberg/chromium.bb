# -*- coding: utf-8 -*-
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Constants related to waterfall names and behaviors."""

WATERFALL_INTERNAL = 'chromeos'
WATERFALL_INFRA = 'chromeos.infra'
WATERFALL_RELEASE = 'chromeos_release'
WATERFALL_BRANCH = 'chromeos.branch'
# Used for all swarming builds.
WATERFALL_SWARMING = 'go/legoland'

# These waterfalls should send email reports regardless of cidb connection.
EMAIL_WATERFALLS = (
    WATERFALL_INTERNAL,
    WATERFALL_RELEASE,
    WATERFALL_BRANCH,
    WATERFALL_SWARMING,
)

# URLs to the various waterfalls.
BUILD_INT_DASHBOARD = 'https://uberchromegw.corp.google.com/i/chromeos'
RELEASE_DASHBOARD = 'https://uberchromegw.corp.google.com/i/chromeos_release'
BRANCH_DASHBOARD = 'https://uberchromegw.corp.google.com/i/chromeos.branch'
SWARMING_DASHBOARD = 'http://go/legoland'

# Waterfall to dashboard URL mapping.
WATERFALL_TO_DASHBOARD = {
    WATERFALL_INTERNAL: BUILD_INT_DASHBOARD,
    WATERFALL_RELEASE: RELEASE_DASHBOARD,
    WATERFALL_BRANCH: BRANCH_DASHBOARD,
    WATERFALL_SWARMING: SWARMING_DASHBOARD,
}

# Sheriff-o-Matic tree which Chrome OS alerts are posted to.
SOM_TREE = 'chromeos'
