# -*- coding: utf-8 -*-
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the uri_lib module."""

from __future__ import print_function

import sys

from chromite.lib import cros_test_lib
from chromite.lib import uri_lib


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


class ShortenUriTests(cros_test_lib.TestCase):
  """Tests for ShortenUri."""

  def testNoMatches(self):
    """Unknown URIs/strings should be left alone"""
    TESTS = (
        'words',
    )
    for test in TESTS:
      self.assertEqual(test, uri_lib.ShortenUri(test))

  def testOmitScheme(self):
    """Check we strip off the scheme"""
    self.assertEqual(
        'crrev.com/c/123',
        uri_lib.ShortenUri('crosreview.com/123', omit_scheme=True))
    self.assertEqual(
        'crrev.com/c/123',
        uri_lib.ShortenUri('https://crosreview.com/123', omit_scheme=True))

  def testGobUris(self):
    """Check GoB URIs using crrev.com"""
    HOST_MAPS = (
        ('chromium', 'c'),
        ('chrome-internal', 'i'),
        ('android', None),
    )
    TESTS = (
        # Basic fragments.
        ('#/662618', '662618'),
        ('#/662618/', '662618'),
        ('#/662618/////', '662618'),
        # Fragments under /c/.
        ('#/c/280497', '280497'),
        ('#/c/280497/', '280497'),
        ('#/c/foo/+/280497/', '280497'),
        # Basic paths.
        ('662618', '662618'),
        ('662618/', '662618'),
        # Extended paths.
        ('c/662618', '662618'),
        ('c/662618/', '662618'),
        ('c/chromiumos/chromite/+/662618', '662618'),
        ('c/chromiumos/chromite/+/662618/', '662618'),
        # Paths with subdirs.
        ('c/662618/1/lib/gs.py', '662618/1/lib/gs.py'),
        ('c/chromiumos/chromite/+/662618/1/lib/gs.py', '662618/1/lib/gs.py'),
    )

    for gob_name, crrev_part in HOST_MAPS:
      for input_part, exp_part in TESTS:
        input_uri = ('https://%s-review.googlesource.com/%s' %
                     (gob_name, input_part))
        if crrev_part is None:
          exp_uri = ('https://%s-review.googlesource.com/%s' %
                     (gob_name, exp_part))
        else:
          exp_uri = 'https://crrev.com/%s/%s' % (crrev_part, exp_part)
        self.assertEqual(exp_uri, uri_lib.ShortenUri(input_uri))

  def testCrosReview(self):
    """Check old review URIs"""
    TESTS = (
        ('https://crrev.com/c/123',
         'https://crosreview.com/123'),
        ('https://crrev.com/i/123',
         'https://crosreview.com/i/123'),
    )
    for exp, uri in TESTS:
      self.assertEqual(exp, uri_lib.ShortenUri(uri))

  def testRietveld(self):
    """Check rietveld URIs"""
    TESTS = (
        ('https://crrev.com/123',
         'https://codereview.chromium.org/123'),
    )
    for exp, uri in TESTS:
      self.assertEqual(exp, uri_lib.ShortenUri(uri))

  def testBuganizerUris(self):
    """Check buganizer URIs"""
    TESTS = (
        'b.corp.google.com/issue?id=123',
        'https://b.corp.google.com/issue?id=123',
        'http://b.corp.google.com/issue?id=123',
        'b.corp.google.com/issue?id=123',
        'b.corp.google.com/issues/123',
    )
    for test in TESTS:
      self.assertEqual('http://b/123', uri_lib.ShortenUri(test))

  def testChromiumUris(self):
    """Check Chromium bug URIs"""
    TESTS = (
        ('https://crbug.com',
         'https://bugs.chromium.org/p/chromium/issues'),
        ('https://crbug.com',
         'https://bugs.chromium.org/p/chromium/issues/list'),
        ('https://crbug.com/123',
         'https://bugs.chromium.org/p/chromium/issues/detail?id=123'),
        ('https://crbug.com/google-breakpad/123',
         'https://bugs.chromium.org/p/google-breakpad/issues/detail?id=123'),
    )
    for exp, uri in TESTS:
      self.assertEqual(exp, uri_lib.ShortenUri(uri))

  def testGutsUris(self):
    """Check GUTS URIs"""
    TESTS = (
        ('http://t/123',
         'https://gutsv3.corp.google.com/#ticket/123'),
        ('http://t/#search/default/My%20Requests/0',
         'https://gutsv3.corp.google.com/#search/default/My%20Requests/0'),
    )
    for exp, uri in TESTS:
      self.assertEqual(exp, uri_lib.ShortenUri(uri))


class ConstructUrlTests(cros_test_lib.TestCase):
  """Tests functions that create URLs."""

  def testConstructMiloBuildURL(self):
    """Tests generating Legoland build URIs."""
    actual = uri_lib.ConstructMiloBuildUri('bbid')
    expected = 'https://ci.chromium.org/b/bbid'

    self.assertEqual(actual, expected)

  def testConstructDashboardUrl(self):
    """Test generating dashboard URIs."""
    actual = uri_lib.ConstructDashboardUri('master', 'builder', 123)
    expected = 'https://luci-milo.appspot.com/buildbot/master/builder/123'
    self.assertEqual(actual, expected)

  def testConstructLogDogUri(self):
    """Test generating LogDog URIs."""
    actual = uri_lib.ConstructLogDogUri(123, 'stage')
    expected = (
        'https://luci-logdog.appspot.com/v/'
        '?s=chromeos/buildbucket/cr-buildbucket.appspot.com/'
        '123/%2B/steps/stage/0/stdout')
    self.assertEqual(actual, expected)

  def testConstructViceroyBuildDetailsUri(self):
    """Test generating Viceroy build details URIs."""
    actual = uri_lib.ConstructViceroyBuildDetailsUri(123)
    expected = (
        'https://viceroy.corp.google.com/chromeos/build_details?build_id=123')
    self.assertEqual(actual, expected)

  def testConstructGoldenEyeBuildDetailsUri(self):
    """Test generating GoldenEye suite details URIs with suite ID."""
    actual = uri_lib.ConstructGoldenEyeBuildDetailsUri(123)
    expected = ('http://go/goldeneye/'
                'chromeos/healthmonitoring/buildDetails?id=123')
    self.assertEqual(actual, expected)
