# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for loading / using build configuration.

*** UPDATE NOTICE ***

Not finding what you're expecting here? Please see generate_chromeos_config.py
for Chrome OS build configuration.
"""

from __future__ import print_function

# These symbols are imported to be exported, not for use.
# pylint: disable=unused-import

from chromite.cbuildbot.generate_chromeos_config import (
    # Classes used to generate build configs.
    BuildConfig,
    HWTestConfig,

    # Fetch the build config.
    GetConfig,
    GetDefault,

    # General constant.
    GS_PATH_DEFAULT,

    # Define config types.
    CONFIG_TYPE_PRECQ,
    CONFIG_TYPE_PALADIN,
    CONFIG_TYPE_RELEASE,
    CONFIG_TYPE_FULL,
    CONFIG_TYPE_FIRMWARE,
    CONFIG_TYPE_FACTORY,
    CONFIG_TYPE_RELEASE_AFDO,

    # Contains the valid build config suffixes in the order that they are
    # dumped.
    CONFIG_TYPE_DUMP_ORDER,

    # Exported method for searching/modifying config information.
    FindCanonicalConfigForBoard,
    FindFullConfigsForBoard,
    GetCanariesForChromeLKGM,
    GetDisplayPosition,
    GetSlavesForMaster,

    # Tests for config type categories.
    IsCanaryType,
    IsCQType,
    IsPFQType,
    )
