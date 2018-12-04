# -*- coding: utf-8 -*-
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Constants related to waterfall names and behaviors."""

WATERFALL_INTERNAL = 'chromeos'
WATERFALL_RELEASE = 'chromeos_release'
# Used for all swarming builds.
WATERFALL_SWARMING = 'go/legoland'

# URLs to the various waterfalls.
BUILD_INT_DASHBOARD = 'https://uberchromegw.corp.google.com/i/chromeos'
SWARMING_DASHBOARD = 'http://go/legoland'

# Sheriff-o-Matic tree which Chrome OS alerts are posted to.
SOM_TREE = 'chromeos'
