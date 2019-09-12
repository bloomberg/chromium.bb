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
    logging.info('Running tests...')

  def setUp(self):
    self.complaints = []
    self.fatal_complaints = []

  def tearDown(self):
    for complaint in self.complaints:
      logging.warn(complaint)
    for complaint in self.fatal_complaints:
      logging.error(complaint)
    if self.fatal_complaints:
      self.fail('Fatal errors found in this test')

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

  def _FindCloseConfigs(self, pfq_configs, config, skipped_useflags):
    """Find configs in |pfq_configs| that are "close" to |config|.

    If there are no prebuilts from any PFQ that match this board, then try to
    help with diagnostics by finding the "closest" matches.

    Args:
      pfq_configs: A PrebuiltMapping object.
      config: The baseline config to compare against.
      skipped_useflags: set of USE flags to ignore when computing changes.

    Returns:
      A list of (BoardKey, CompatId, <flag source>, added-flags, removed-flags)
      sorted by the number of changed flags.
    """
    compat_id = self.GetCompatId(config)

    ret = []
    for close_id, board_key in pfq_configs.by_compat_id.items():
      # Only consider matching architectures.
      if compat_id.arch == close_id.arch:
        added = (set(close_id.useflags) - set(compat_id.useflags) -
                 skipped_useflags)
        removed = (set(compat_id.useflags) - set(close_id.useflags) -
                   skipped_useflags)
        if added or removed:
          ret.append((board_key, compat_id, 'USE',
                      sorted('+%s' % x for x in added),
                      sorted('-%s' % x for x in removed)))
        else:
          added = set(close_id.cflags) - set(compat_id.cflags)
          removed = set(close_id.cflags) - set(compat_id.cflags)
          ret.append((board_key, compat_id, 'CFLAGS',
                      sorted('+%s' % x for x in added),
                      sorted('-%s' % x for x in removed)))

    # Do the final sort of the configs based on number of USE changes.
    return sorted(ret, key=lambda x: len(x[3]) + len(x[4]))

  def AssertChromePrebuilts(self, pfq_configs, config, skip_useflags=False):
    """Verify that the specified config has Chrome prebuilts.

    Args:
      pfq_configs: A PrebuiltMapping object.
      config: The config to check.
      skip_useflags: Don't use extra useflags from the config.
    """
    # Skip over useflags from the useflag if needed.
    msg_prefix = ''
    skipped_useflags = set()
    if skip_useflags and config.useflags:
      skipped_useflags = set(config.useflags)
      msg_prefix = ('When we take out config-requested useflags %s for '
                    'public/partner builds, '
                    % (tuple(x.encode('ascii') for x in config.useflags),))
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
          msg = '%s%s uses mismatched Chrome prebuilts from %s\n\t%s'
          self.Complain(msg % (msg_prefix, config.name, key.board, err),
                        fatal=False)
          pfqs.add(key)

    if not pfqs:
      pre_cq = (config.build_type == config_lib.CONFIG_TYPE_PRECQ)
      if pre_cq:
        fatal_scope = 'pre_cq'
      elif config.important:
        fatal_scope = 'important'
      else:
        fatal_scope = 'non-fatal'
      msg = ('%s%s (%s) cannot find Chrome prebuilts (probably due to USE flag '
             'mismatch)\nBuild settings: %s'
             % (msg_prefix, config.name, fatal_scope, compat_id))

      # For brevity, we only show the first three closest matches.  After that,
      # we start getting redundant, and the deltas get larger.  This is just a
      # debug display, so it need not be perfect.
      close_configs = self._FindCloseConfigs(pfq_configs, config,
                                             skipped_useflags)
      if close_configs:
        msg += '\nClosest matching configs:\n'
        for board_key, compat_id, source, added, removed in close_configs[0:6]:
          msg += ('\tBoards: %s\n\t\t%s changes: %s\n\t\tBuild settings: %s\n'
                  % (board_key, source, added + removed, compat_id))

      self.Complain(msg, fatal=pre_cq or config.important)

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
    """Verify all builds that use Chrome have matching Chrome PFQ configs.

    Args:
      filename: Filename to load our PFQ mappings from. By default, generate
        the PFQ mappings based on the current config.
    """
    if filename is not None:
      logging.info('Checking PFQ database: %s', filename)
      pfq_configs = binhost.PrebuiltMapping.Load(filename)
    else:
      logging.info('Checking config_lib.GetConfig().site_config')
      keys = binhost.GetChromePrebuiltConfigs(self.site_config).keys()
      pfq_configs = binhost.PrebuiltMapping.Get(keys, self.COMPAT_IDS)

    for compat_id, pfqs in sorted(pfq_configs.by_compat_id.items(),
                                  key=lambda x: str(x[1])):
      if len(pfqs) > 1:
        self.Complain(
            'The following Chrome PFQs produce identical prebuilts:\n'
            '\t%s\n\t%s'
            % ('\n\t'.join(sorted(str(x) for x in pfqs)), compat_id),
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
    """Verify Chrome prebuilts actually exist for all configs that build Chrome.

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
