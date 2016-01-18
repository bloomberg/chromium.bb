#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions for dealing with crash reports."""

import logging
import os
_LOGGER = logging.getLogger(os.path.basename(__file__))


def LogCrashKeys(report):
  for key in sorted(report.keys()):
    val = report[key][0]
    if (len(val) < 64):
      _LOGGER.debug('Got crashkey "%s": "%s"', key, val)
    else:
      _LOGGER.debug('Got crashkey "%s": ...%d bytes...', key, len(val))


def ValidateCrashReport(report, expectations=None):
  expected_keys = {}

  # The following keys are all expected to be set in all crashes, and should
  # be set by GetCrashKeysForKasko.
  get_crash_keys = 'GetCrashKeysForKasko'
  for k in ['guid', 'prod', 'plat', 'ver', 'ptype', 'channel']:
    expected_keys[k] = get_crash_keys

  # The following crash keys are expected to be set by the Kasko code itself.
  kasko = 'Kasko'
  for k in ['kasko-generated-by-version', 'kasko-uploaded-by-version']:
    expected_keys[k] = kasko

  # Merge in additional expectations.
  if expectations:
    for key, value in expectations.iteritems():
      expected_keys[key] = value

  # Validate the expectations.
  for expected_key, error in expected_keys.iteritems():
    if expected_key not in report:
      _LOGGER.error('Missing expected "%s" crash key.', expected_key)
      raise Exception('"%s" integration appears broken.' % error)
