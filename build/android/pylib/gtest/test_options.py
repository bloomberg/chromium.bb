# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Defines the GTestOptions named tuple."""

import collections

GTestOptions = collections.namedtuple('GTestOptions', [
    'tool',
    'cleanup_test_files',
    'push_deps',
    'gtest_filter',
    'run_disabled',
    'test_arguments',
    'timeout',
    'suite_name'])
