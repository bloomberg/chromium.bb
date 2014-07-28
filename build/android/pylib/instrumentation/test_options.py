# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Defines the InstrumentationOptions named tuple."""

import collections

InstrumentationOptions = collections.namedtuple('InstrumentationOptions', [
    'tool',
    'cleanup_test_files',
    'push_deps',
    'annotations',
    'exclude_annotations',
    'test_filter',
    'test_data',
    'test_runner',
    'save_perf_json',
    'screenshot_failures',
    'wait_for_debugger',
    'coverage_dir',
    'test_apk',
    'test_apk_path',
    'test_apk_jar_path',
    'test_support_apk_path'])
