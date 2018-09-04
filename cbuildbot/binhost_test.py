# -*- coding: utf-8 -*-
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for verifying prebuilts."""

from __future__ import print_function

import inspect
import os
import unittest

from chromite.cbuildbot import binhost
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import parallel


class PrebuiltCompatibilityTest(cros_test_lib.TestCase):
  """Ensure that prebuilts are present for all builders and are compatible."""

  # Whether to cache setup from run to run. If set, requires that you install
  # joblib (sudo easy_install joblib). This is useful for iterating on the
  # unit tests, but note that if you 'repo sync', you'll need to clear out
  # /tmp/joblib and blow away /build in order to update the caches. Note that
  # this is never normally set to True -- if you want to use this feature,
  # you'll need to hand-edit this file.
  # TODO(davidjames): Add a --caching option.
  CACHING = False

  # A dict mapping BoardKeys to their associated compat ids.
  COMPAT_IDS = None

  # Boards that don't have Chromium PFQs.
  # TODO(davidjames): Empty this list.
  BOARDS_WITHOUT_CHROMIUM_PFQS = ['veyron_rialto']

  site_config = config_lib.GetConfig()

  @classmethod
  def setUpClass(cls):
    assert cros_build_lib.IsInsideChroot()
    logging.info('Generating board configs.')
    board_keys = binhost.GetAllImportantBoardKeys(cls.site_config)
    boards = set(key.board for key in board_keys)
    inputs = [[board, not cls.CACHING, False] for board in boards]
    parallel.RunTasksInProcessPool(binhost.GenConfigsForBoard, inputs)
    fetcher = binhost.CompatIdFetcher(caching=cls.CACHING)
    cls.COMPAT_IDS = fetcher.FetchCompatIds(list(board_keys))

  def setUp(self):
    self.complaints = []
    self.fatal_complaints = []

  def tearDown(self):
    if self.complaints:
      logging.warn('\n' + '\n'.join(self.complaints))
    if self.fatal_complaints:
      self.assertFalse(self.fatal_complaints, '\n'.join(self.fatal_complaints))

  def Complain(self, msg, fatal):
    """Complain about an error when the test exits.

    Args:
      msg: The message to print.
      fatal: Whether the message should be fatal. If not, the message will be
        considered a warning.
    """
    if fatal:
      self.fatal_complaints.append(msg)
    else:
      self.complaints.append(msg)

  def GetCompatIdDiff(self, expected, actual):
    """Return a string describing the differences between expected and actual.

    Args:
      expected: Expected value for CompatId.
      actual: Actual value for CompatId.
    """
    if expected.arch != actual.arch:
      return 'arch differs: %s != %s' % (expected.arch, actual.arch)
    elif expected.useflags != actual.useflags:
      msg = self.GetSequenceDiff(expected.useflags, actual.useflags)
      return msg.replace('Sequences', 'useflags')
    elif expected.cflags != actual.cflags:
      msg = self.GetSequenceDiff(expected.cflags, actual.cflags)
      return msg.replace('Sequences', 'cflags')
    else:
      assert expected == actual
      return 'no differences'

  def AssertChromePrebuilts(self, pfq_configs, config, skip_useflags=False):
    """Verify that the specified config has Chrome prebuilts.

    Args:
      pfq_configs: A PrebuiltMapping object.
      config: The config to check.
      skip_useflags: Don't use extra useflags from the config.
    """
    # Skip over useflags from the useflag if needed.
    msg_prefix = ''
    if skip_useflags and config.useflags:
      msg_prefix = 'When we take out config-requested useflags (%s), ' % (
          config.useflags)
      config = config.deepcopy()
      config.useflags = []

    compat_id = self.GetCompatId(config)
    pfqs = pfq_configs.by_compat_id.get(compat_id, set())
    if not pfqs:
      arch_useflags = (compat_id.arch, compat_id.useflags)
      for key in pfq_configs.by_arch_useflags[arch_useflags]:
        # If there wasn't an exact match for this CompatId, but there
        # was an (arch, useflags) match, then we'll be using mismatched
        # Chrome prebuilts. Complain.
        # TODO(davidjames): This should be a fatal error for important
        # builders, but we need to clean up existing cases first.
        pfq_compat_id = self.COMPAT_IDS.get(key)
        if pfq_compat_id:
          err = self.GetCompatIdDiff(compat_id, pfq_compat_id)
          msg = '%s%s uses mismatched Chrome prebuilts from %s -- %s'
          self.Complain(msg % (msg_prefix, config.name, key.board, err),
                        fatal=False)
          pfqs.add(key)

    if not pfqs:
      pre_cq = (config.build_type == config_lib.CONFIG_TYPE_PRECQ)
      msg = '%s%s cannot find Chrome prebuilts -- %s'
      self.Complain(msg % (msg_prefix, config.name, compat_id),
                    fatal=pre_cq or config.important)

  def GetCompatId(self, config, board=None):
    """Get the CompatId for a config.

    Args:
      config: A config_lib.BuildConfig object.
      board: Board to use. Defaults to the first board in the config.
          Optional if len(config.boards) == 1.
    """
    if board is None:
      assert len(config.boards) == 1
      board = config.boards[0]
    else:
      assert board in config.boards

    board_key = binhost.GetBoardKey(config, board)
    compat_id = self.COMPAT_IDS.get(board_key)
    if compat_id is None:
      compat_id = binhost.CalculateCompatId(board, config.useflags)
      self.COMPAT_IDS[board_key] = compat_id
    return compat_id

  def _GuessActiveConfigs(self):
    """Guess at which build configs are artively used.

    LUCI Scheduler's config is the source of truth, but that's
    not available here, so take a guess at "good enough".
    See crbug.com/831929

    Returns:
      A map of build configs. { name: config }
    """
    result = {}
    for name, config in self.site_config.items():
      if config.master and config.important:
        result[name] = config
        for s in config.slave_configs:
          result[s] = self.site_config[s]

    return result

  def testChromePrebuiltsPresent(self, filename=None):
    """Verify Chrome prebuilts exist for all configs that build Chrome.

    Args:
      filename: Filename to load our PFQ mappings from. By default, generate
        the PFQ mappings based on the current config.
    """
    if filename is not None:
      pfq_configs = binhost.PrebuiltMapping.Load(filename)
    else:
      keys = binhost.GetChromePrebuiltConfigs(self.site_config).keys()
      pfq_configs = binhost.PrebuiltMapping.Get(keys, self.COMPAT_IDS)

    for compat_id, pfqs in pfq_configs.by_compat_id.items():
      if len(pfqs) > 1:
        msg = 'The following Chrome PFQs produce identical prebuilts: %s -- %s'
        self.Complain(msg % (', '.join(str(x) for x in pfqs), compat_id),
                      fatal=False)

    # Sort the names to ensure consistent errors.
    for _name, config in sorted(self._GuessActiveConfigs().items()):

      # Skip over configs that don't have Chrome or have >1 board.
      if config.sync_chrome is False or len(config.boards) != 1:
        continue

      # Look for boards with missing prebuilts.
      if config.usepkg_build_packages and not config.chrome_rev:
        self.AssertChromePrebuilts(pfq_configs, config)

        # Check that we have a builder for the version w/o custom useflags as
        # well.
        if (config.useflags and
            config.boards[0] not in self.BOARDS_WITHOUT_CHROMIUM_PFQS):
          self.AssertChromePrebuilts(pfq_configs, config, skip_useflags=True)

  def testCurrentChromePrebuiltsEnough(self):
    """Verify Chrome prebuilts exist for all configs that build Chrome.

    This loads the list of Chrome prebuilts that were generated during the last
    Chrome PFQ run from disk and verifies that it is sufficient.
    """
    filename = binhost.PrebuiltMapping.GetFilename(constants.SOURCE_ROOT,
                                                   'chrome')
    if os.path.exists(filename):
      self.testChromePrebuiltsPresent(filename)

  def testDumping(self):
    """Verify Chrome prebuilts exist for all configs that build Chrome.

    This loads the list of Chrome prebuilts that were generated during the last
    Chrome PFQ run from disk and verifies that it is sufficient.
    """
    with osutils.TempDir() as tempdir:
      keys = binhost.GetChromePrebuiltConfigs(self.site_config).keys()
      pfq_configs = binhost.PrebuiltMapping.Get(keys, self.COMPAT_IDS)
      filename = os.path.join(tempdir, 'foo.json')
      pfq_configs.Dump(filename)
      self.assertEqual(pfq_configs, binhost.PrebuiltMapping.Load(filename))


def NoIncremental():
  """Creates a suite containing only non-incremental tests.

  This suite should be used on the Chrome PFQ as we don't need to preserve
  incremental compatibility of prebuilts.

  Returns:
    A unittest.TestSuite that does not contain any incremental tests.
  """
  suite = unittest.TestSuite()
  method_names = [f[0] for f in inspect.getmembers(PrebuiltCompatibilityTest,
                                                   predicate=inspect.ismethod)]
  for m in method_names:
    if m.startswith('test') and m != 'testCurrentChromePrebuiltsEnough':
      suite.addTest(PrebuiltCompatibilityTest(m))
  return suite
