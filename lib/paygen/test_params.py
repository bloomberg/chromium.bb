# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Classes for holding autoupdate_EndToEnd test parameters."""

from __future__ import print_function

from chromite.lib.paygen import test_control


_DEFAULT_AU_SUITE_NAME = 'au'


class TestConfig(object):
  """A single test configuration.

  Stores and generates arguments for running autotest_EndToEndTest.
  """

  def __init__(self, board, name, is_delta_update, source_release,
               target_release, source_payload_uri, target_payload_uri,
               suite_name=_DEFAULT_AU_SUITE_NAME, source_archive_uri=None,
               payload_type=None, applicable_models=None):
    """Initialize a test configuration.

    Args:
      board: the board being tested (e.g. 'x86-alex')
      name: a descriptive name of the test
      is_delta_update: whether this is a delta update test (Boolean)
      source_release: the source image version (e.g. '2672.0.0')
      target_release: the target image version (e.g. '2673.0.0')
      source_payload_uri: source payload URI ('gs://...') or None
      target_payload_uri: target payload URI ('gs://...')
      suite_name: the name of the test suite (default: 'au')
      source_archive_uri: location of source build artifacts
      payload_type: The type of update we are doing with this payload.
                    Possible types are in defined in PAYLOAD_TYPES at
                    chromite/lib/paygen/paygen_build_lib.
      applicable_models: A list of models that this config should run
                         against. Only used for FSI configs. None
                         indicates it can run on any board.
    """
    self.board = board
    self.name = name
    self.is_delta_update = is_delta_update
    self.source_release = source_release
    self.target_release = target_release
    self.source_payload_uri = source_payload_uri
    self.target_payload_uri = target_payload_uri
    self.suite_name = suite_name
    self.source_archive_uri = source_archive_uri
    self.payload_type = payload_type
    self.applicable_models = applicable_models

  def get_update_type(self):
    return 'delta' if self.is_delta_update else 'full'

  def unique_name_suffix(self):
    """Unique name suffix for the test config given the target version."""

    payload_type_ending = ''
    if self.payload_type:
      payload_type_ending = '_%s' % self.payload_type.lower()
    return '%s_%s_%s%s' % (self.name,
                           'delta' if self.is_delta_update else 'full',
                           self.source_release,
                           payload_type_ending)

  def get_autotest_name(self):
    """Returns job name to use when creating an autotest job.

    Returns:
      A job name that conforms to the suite naming style.
    """
    return '%s-release/%s/%s/%s.%s' % (
        self.board, self.target_release, self.suite_name,
        test_control.get_test_name(), self.unique_name_suffix())

  def get_control_file_name(self):
    """Returns the name of the name of the control file to store this in.

    Return:
      The control file name that should be generated for this test.
      A unique name suffix is used to keep from collisions per target
      release/board.
    """
    return 'control.%s' % self.unique_name_suffix()

  def __str__(self):
    """Short textual representation w/o image/payload URIs."""
    return ('[%s/%s/%s/%s -> %s]' %
            (self.board, self.name, self.get_update_type(),
             self.source_release, self.target_release))

  def __repr__(self):
    """Full textual representation w/ image/payload URIs."""
    return '\n'.join([str(self),
                      'source payload : %s' % self.source_payload_uri,
                      'target payload : %s' % self.target_payload_uri])

  def _get_args(self, assign, delim, is_quote_val):
    template = "%s%s'%s'" if is_quote_val else '%s%s%s'
    arg_values = [
        ('name', self.name),
        ('update_type', self.get_update_type()),
        ('source_release', self.source_release),
        ('target_release', self.target_release),
        ('target_payload_uri', self.target_payload_uri),
        ('SUITE', self.suite_name),
    ]

    if self.source_payload_uri:
      arg_values.append(('source_payload_uri', self.source_payload_uri))
    if self.source_archive_uri:
      arg_values.append(('source_archive_uri', self.source_archive_uri))
    if self.payload_type:
      arg_values.append(('payload_type', self.payload_type))

    return delim.join(
        template % (key, assign, val) for key, val in arg_values)

  def get_cmdline_args(self):
    return self._get_args('=', ' ', False)

  def get_code_args(self):
    args = self._get_args(' = ', '\n', True)
    return args + '\n' if args else ''
