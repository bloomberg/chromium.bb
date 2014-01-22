#!/usr/bin/python

# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the cbuildbot_archive module."""

import logging
import os
import sys

sys.path.insert(0, os.path.abspath('%s/../..' % os.path.dirname(__file__)))
from chromite.buildbot import cbuildbot_archive
from chromite.buildbot import cbuildbot_config
from chromite.buildbot import cbuildbot_run
from chromite.lib import cros_test_lib

import mock

DEFAULT_ARCHIVE_PREFIX = 'bogus_bucket/TheArchiveBase'
DEFAULT_ARCHIVE_BASE = 'gs://%s' % DEFAULT_ARCHIVE_PREFIX
DEFAULT_BUILDROOT = '/tmp/foo/bar/buildroot'
DEFAULT_BUILDNUMBER = 12345
DEFAULT_BRANCH = 'TheBranch'
DEFAULT_CHROME_BRANCH = 'TheChromeBranch'
DEFAULT_VERSION_STRING = 'TheVersionString'
DEFAULT_BOARD = 'TheBoard'
DEFAULT_BOT_NAME = 'TheCoolBot'

# Access to protected member.
# pylint: disable=W0212

DEFAULT_OPTIONS = cros_test_lib.EasyAttr(
    archive_base=DEFAULT_ARCHIVE_BASE,
    buildroot=DEFAULT_BUILDROOT,
    buildnumber=DEFAULT_BUILDNUMBER,
    buildbot=True,
    branch=DEFAULT_BRANCH,
    remote_trybot=False,
    debug=False,
)
DEFAULT_CONFIG = cbuildbot_config._config(
    name=DEFAULT_BOT_NAME,
    master=True,
    boards=[DEFAULT_BOARD],
    child_configs=[cbuildbot_config._config(name='foo'),
                   cbuildbot_config._config(name='bar'),
                  ],
)


def _ExtendDefaultOptions(**kwargs):
  """Extend DEFAULT_OPTIONS with keys/values in kwargs."""
  options_kwargs = DEFAULT_OPTIONS.copy()
  options_kwargs.update(kwargs)
  return cros_test_lib.EasyAttr(**options_kwargs)


def _ExtendDefaultConfig(**kwargs):
  """Extend DEFAULT_CONFIG with keys/values in kwargs."""
  config_kwargs = DEFAULT_CONFIG.copy()
  config_kwargs.update(kwargs)
  return cbuildbot_config._config(**config_kwargs)


def _NewBuilderRun(options=None, config=None):
  """Create a BuilderRun objection from options and config values.

  Args:
    options: Specify options or default to DEFAULT_OPTIONS.
    config: Specify build config or default to DEFAULT_CONFIG.

  Returns:
    BuilderRun object.
  """
  # Make up a fake object with a Queue() method.
  class _FakeMultiprocessManager(object):
    """This just needs to not crash when Queue/RLock called."""
    def Queue(self):
      return 'SomeQueue'
    def RLock(self):
      return 'SomeLock'
  manager = _FakeMultiprocessManager()

  options = options or DEFAULT_OPTIONS
  config = config or DEFAULT_CONFIG
  return cbuildbot_run.BuilderRun(options, config, manager)


class GetBaseUploadURITest(cros_test_lib.TestCase):
  """Test the GetBaseUploadURI function."""

  ARCHIVE_BASE = '/tmp/the/archive/base'
  BOT_ID = 'TheNewBotId'

  def setUp(self):
    self.cfg = DEFAULT_CONFIG

  def _GetBaseUploadURI(self, *args, **kwargs):
    """Test GetBaseUploadURI with archive_base and no bot_id."""
    return cbuildbot_archive.GetBaseUploadURI(self.cfg, *args, **kwargs)

  def testArchiveBaseRemoteTrybotFalse(self):
    expected_result = '%s/%s' % (self.ARCHIVE_BASE, DEFAULT_BOT_NAME)
    result = self._GetBaseUploadURI(archive_base=self.ARCHIVE_BASE,
                                    remote_trybot=False)
    self.assertEqual(expected_result, result)

  def testArchiveBaseRemoteTrybotTrue(self):
    expected_result = '%s/trybot-%s' % (self.ARCHIVE_BASE, DEFAULT_BOT_NAME)
    result = self._GetBaseUploadURI(archive_base=self.ARCHIVE_BASE,
                                    remote_trybot=True)
    self.assertEqual(expected_result, result)

  def testArchiveBaseBotIdRemoteTrybotFalse(self):
    expected_result = '%s/%s' % (self.ARCHIVE_BASE, self.BOT_ID)
    result = self._GetBaseUploadURI(archive_base=self.ARCHIVE_BASE,
                                    bot_id=self.BOT_ID, remote_trybot=False)
    self.assertEqual(expected_result, result)

  def testArchiveBaseBotIdRemoteTrybotTrue(self):
    expected_result = '%s/%s' % (self.ARCHIVE_BASE, self.BOT_ID)
    result = self._GetBaseUploadURI(archive_base=self.ARCHIVE_BASE,
                                    bot_id=self.BOT_ID, remote_trybot=True)
    self.assertEqual(expected_result, result)

  def testRemoteTrybotTrue(self):
    """Test GetBaseUploadURI with no archive base but remote_trybot is True."""
    expected_result = ('%s/trybot-%s' %
                       (cbuildbot_archive.constants.DEFAULT_ARCHIVE_BUCKET,
                        DEFAULT_BOT_NAME))
    result = self._GetBaseUploadURI(remote_trybot=True)
    self.assertEqual(expected_result, result)

  def testBotIdRemoteTrybotTrue(self):
    expected_result = ('%s/%s' %
                       (cbuildbot_archive.constants.DEFAULT_ARCHIVE_BUCKET,
                        self.BOT_ID))
    result = self._GetBaseUploadURI(bot_id=self.BOT_ID, remote_trybot=True)
    self.assertEqual(expected_result, result)

  def testDefaultGSPathRemoteTrybotFalse(self):
    """Test GetBaseUploadURI with default gs_path value in config."""
    self.cfg = _ExtendDefaultConfig(gs_path=cbuildbot_config.GS_PATH_DEFAULT)

    # Test without bot_id.
    expected_result = ('%s/%s' %
                       (cbuildbot_archive.constants.DEFAULT_ARCHIVE_BUCKET,
                        DEFAULT_BOT_NAME))
    result = self._GetBaseUploadURI(remote_trybot=False)
    self.assertEqual(expected_result, result)

    # Test with bot_id.
    expected_result = ('%s/%s' %
                       (cbuildbot_archive.constants.DEFAULT_ARCHIVE_BUCKET,
                        self.BOT_ID))
    result = self._GetBaseUploadURI(bot_id=self.BOT_ID, remote_trybot=False)
    self.assertEqual(expected_result, result)

  def testOverrideGSPath(self):
    """Test GetBaseUploadURI with default gs_path value in config."""
    self.cfg = _ExtendDefaultConfig(gs_path='gs://funkytown/foo/bar')

    # Test without bot_id.
    expected_result = self.cfg.gs_path
    result = self._GetBaseUploadURI(remote_trybot=False)
    self.assertEqual(expected_result, result)

    # Test with bot_id.
    expected_result = self.cfg.gs_path
    result = self._GetBaseUploadURI(bot_id=self.BOT_ID, remote_trybot=False)
    self.assertEqual(expected_result, result)


class ArchiveTest(cros_test_lib.TestCase):
  """Test the Archive class."""
  _VERSION = '6543.2.1'

  def _GetAttributeValue(self, attr, options=None, config=None):
    with mock.patch.object(cbuildbot_run._BuilderRunBase, 'GetVersion') as m:
      m.return_value = self._VERSION

      run = _NewBuilderRun(options, config)
      return getattr(run.GetArchive(), attr)

  def testVersion(self):
    value = self._GetAttributeValue('version')
    self.assertEqual(self._VERSION, value)

  def testVersionNotReady(self):
    run = _NewBuilderRun()
    self.assertRaises(AttributeError, getattr, run, 'version')

  def testArchivePathTrybot(self):
    options = _ExtendDefaultOptions(buildbot=False)
    value = self._GetAttributeValue('archive_path', options=options)
    expected_value = ('%s/%s/%s/%s' %
                      (DEFAULT_BUILDROOT,
                       cbuildbot_archive.Archive._TRYBOT_ARCHIVE,
                       DEFAULT_BOT_NAME,
                       self._VERSION))
    self.assertEqual(expected_value, value)

  def testArchivePathBuildbot(self):
    value = self._GetAttributeValue('archive_path')
    expected_value = ('%s/%s/%s/%s' %
                      (DEFAULT_BUILDROOT,
                       cbuildbot_archive.Archive._BUILDBOT_ARCHIVE,
                       DEFAULT_BOT_NAME,
                       self._VERSION))
    self.assertEqual(expected_value, value)

  def testUploadUri(self):
    value = self._GetAttributeValue('upload_url')
    expected_value = '%s/%s/%s' % (DEFAULT_ARCHIVE_BASE,
                                   DEFAULT_BOT_NAME,
                                   self._VERSION)
    self.assertEqual(expected_value, value)

  def testDownloadURLBuildbot(self):
    value = self._GetAttributeValue('download_url')
    expected_value = ('%s%s/%s/%s' %
                      (cbuildbot_archive.gs.PRIVATE_BASE_HTTPS_URL,
                       DEFAULT_ARCHIVE_PREFIX,
                       DEFAULT_BOT_NAME,
                       self._VERSION))
    self.assertEqual(expected_value, value)




if __name__ == '__main__':
  cros_test_lib.main(level=logging.DEBUG)
