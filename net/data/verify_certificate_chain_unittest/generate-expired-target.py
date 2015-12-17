#!/usr/bin/python
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Certificate chain with 1 intermediary, where the target is expired (violates
validity.notAfter). Verification is expected to fail."""

import common

# Self-signed root certificate (part of trust store).
root = common.create_self_signed_root_certificate('Root')

# Intermediary certificate.
intermediary = common.create_intermediary_certificate('Intermediary', root)

# Target certificate.
target = common.create_end_entity_certificate('Target', intermediary)
target.set_validity_range(common.JANUARY_1_2015_UTC, common.JANUARY_1_2016_UTC)

chain = [target, intermediary]
trusted = [root]

# March 2nd, 2016 midnight UTC
time = '160302120000Z'
verify_result = False

common.write_test_file(__doc__, chain, trusted, time, verify_result)
