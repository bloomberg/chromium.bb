# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Hatch configs."""

from __future__ import print_function

BUILD_WORKON_PACKAGES = (
    'chromeos-ec',
    'coreboot',
    'depthcharge',
    'libpayload',
    'vboot_reference',
)

BUILD_PACKAGES = BUILD_WORKON_PACKAGES + (
    'chromeos-bootimage',
    'coreboot-private-files',
    'coreboot-private-files-hatch',
    'intel-cmlfsp',
)
