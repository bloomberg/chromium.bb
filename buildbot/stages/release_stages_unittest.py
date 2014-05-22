#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for build stages."""

import os
import sys
import unittest

sys.path.insert(0, os.path.abspath('%s/../../..' % os.path.dirname(__file__)))
from chromite.buildbot.stages import artifact_stages
from chromite.buildbot.stages import generic_stages_unittest
from chromite.buildbot.stages import release_stages
from chromite.lib import cros_test_lib
from chromite.lib import timeout_util

from chromite.buildbot.stages.generic_stages_unittest import patch
from chromite.buildbot.stages.generic_stages_unittest import BuilderRunMock

try:
  # pylint: disable=F0401
  from crostools.lib import gspaths
  from crostools.lib import paygen_build_lib
  CROSTOOLS_AVAILABLE = True
except ImportError:
  CROSTOOLS_AVAILABLE = False

# TODO(build): Finish test wrapper (http://crosbug.com/37517).
# Until then, this has to be after the chromite imports.
import mock

# pylint: disable=R0901, W0212
class PaygenStageTest(generic_stages_unittest.AbstractStageTest):
  """Test the PaygenStageStage."""
  BOT_ID = 'x86-mario-release'
  RELEASE_TAG = '0.0.1'

  SIGNER_RESULT = """
    { "status": { "status": "passed" }, "board": "link",
    "keyset": "link-mp-v4", "type": "recovery", "channel": "stable" }
    """

  INSNS_URLS_PER_CHANNEL = {
      'chan1': ['chan1_uri1', 'chan1_uri2'],
      'chan2': ['chan2_uri1'],
  }

  def setUp(self):
    self.StartPatcher(BuilderRunMock())
    self._Prepare()

  def ConstructStage(self):
    archive_stage = artifact_stages.ArchiveStage(self._run, self._current_board)
    return release_stages.PaygenStage(self._run, self._current_board,
                                      archive_stage)

  def testWaitForPushImageSuccess(self):
    """Test waiting for input from PushImage."""
    stage = self.ConstructStage()
    stage.board_runattrs.SetParallel(
        'instruction_urls_per_channel', self.INSNS_URLS_PER_CHANNEL)

    self.assertEqual(stage._WaitForPushImage(), self.INSNS_URLS_PER_CHANNEL)

  def testWaitForPushImageTimeout(self):
    """Test that WaitForPushImage times out, if pushimage never runs."""
    stage = self.ConstructStage()

    # Shorten the timeout so the tests finish in a reasonable period of time.
    stage.PUSHIMAGE_TIMEOUT = 0.01

    self.assertRaises(release_stages.MissingInstructionException,
                      stage._WaitForPushImage)

  def testWaitForPushImageError(self):
    """Test WaitForPushImageError with an error output from pushimage."""
    stage = self.ConstructStage()
    stage.board_runattrs.SetParallel(
        'instruction_urls_per_channel', None)

    self.assertRaises(release_stages.MissingInstructionException,
                      stage._WaitForPushImage)

  def testWaitForSigningResultsSuccess(self):
    """Test that SignerResultsStage works when signing works."""
    results = ['chan1_uri1.json', 'chan1_uri2.json', 'chan2_uri1.json']

    with patch(release_stages.gs, 'GSContext') as mock_gs_ctx_init:
      mock_gs_ctx = mock_gs_ctx_init.return_value
      mock_gs_ctx.Cat.return_value.output = self.SIGNER_RESULT
      notifier = mock.Mock()

      stage = self.ConstructStage()
      stage._WaitForSigningResults(self.INSNS_URLS_PER_CHANNEL, notifier)

      self.assertEqual(notifier.mock_calls,
                       [mock.call('chan1'),
                        mock.call('chan2')])

      for result in results:
        mock_gs_ctx.Cat.assert_any_call(result)

  def testWaitForSigningResultsSuccessNothingSigned(self):
    """Test that SignerResultsStage passes when there are no signed images."""
    with patch(release_stages.gs, 'GSContext') as mock_gs_ctx_init:
      mock_gs_ctx = mock_gs_ctx_init.return_value
      mock_gs_ctx.Cat.return_value.output = self.SIGNER_RESULT
      notifier = mock.Mock()

      stage = self.ConstructStage()
      stage._WaitForSigningResults({}, notifier)

      self.assertEqual(notifier.mock_calls, [])
      self.assertEqual(mock_gs_ctx.Cat.mock_calls, [])

  def testWaitForSigningResultsFailure(self):
    """Test that SignerResultsStage errors when the signers report an error."""
    with patch(release_stages.gs, 'GSContext') as mock_gs_ctx_init:
      mock_gs_ctx = mock_gs_ctx_init.return_value
      mock_gs_ctx.Cat.return_value.output = """
          { "status": { "status": "failed" }, "board": "link",
            "keyset": "link-mp-v4", "type": "recovery", "channel": "stable" }
          """
      notifier = mock.Mock()

      stage = self.ConstructStage()

      self.assertRaises(release_stages.SignerFailure,
                        stage._WaitForSigningResults,
                        {'chan1': ['chan1_uri1']}, notifier)

      self.assertEqual(notifier.mock_calls, [])
      self.assertEqual(mock_gs_ctx.Cat.mock_calls,
                       [mock.call('chan1_uri1.json')])

  def testWaitForSigningResultsMalformedJson(self):
    """Test that SignerResultsStage errors when invalid Json is received.."""
    with patch(release_stages.gs, 'GSContext') as mock_gs_ctx_init:
      mock_gs_ctx = mock_gs_ctx_init.return_value
      mock_gs_ctx.Cat.return_value.output = "{"
      notifier = mock.Mock()

      stage = self.ConstructStage()

      self.assertRaises(release_stages.MalformedResultsException,
                        stage._WaitForSigningResults,
                        self.INSNS_URLS_PER_CHANNEL, notifier)

      self.assertEqual(notifier.mock_calls, [])
      self.assertEqual(mock_gs_ctx.Cat.mock_calls,
                       [mock.call('chan1_uri1.json')])

  def testWaitForSigningResultsTimeout(self):
    """Test that SignerResultsStage reports timeouts correctly."""
    with patch(release_stages.timeout_util, 'WaitForSuccess') as mock_wait:
      mock_wait.side_effect = timeout_util.TimeoutError
      notifier = mock.Mock()

      stage = self.ConstructStage()

      self.assertRaises(release_stages.SignerResultsTimeout,
                        stage._WaitForSigningResults,
                        {'chan1': ['chan1_uri1']}, notifier)

      self.assertEqual(notifier.mock_calls, [])

  def testCheckForResultsSuccess(self):
    """Test that SignerResultsStage works when signing works."""
    with patch(release_stages.gs, 'GSContext') as mock_gs_ctx_init:
      mock_gs_ctx = mock_gs_ctx_init.return_value
      mock_gs_ctx.Cat.return_value.output = self.SIGNER_RESULT
      notifier = mock.Mock()

      stage = self.ConstructStage()
      self.assertTrue(
          stage._CheckForResults(mock_gs_ctx,
                                 self.INSNS_URLS_PER_CHANNEL,
                                 notifier))
      self.assertEqual(notifier.mock_calls,
                       [mock.call('chan1'), mock.call('chan2')])

  def testCheckForResultsSuccessNoChannels(self):
    """Test that SignerResultsStage works when there is nothing to check for."""
    with patch(release_stages.gs, 'GSContext') as mock_gs_ctx_init:
      mock_gs_ctx = mock_gs_ctx_init.return_value
      notifier = mock.Mock()

      stage = self.ConstructStage()

      # Ensure we find that we are ready if there are no channels to look for.
      self.assertTrue(stage._CheckForResults(mock_gs_ctx, {}, notifier))

      # Ensure we didn't contact GS while checking for no channels.
      self.assertFalse(mock_gs_ctx.Cat.called)
      self.assertEqual(notifier.mock_calls, [])

  def testCheckForResultsPartialComplete(self):
    """Verify _CheckForResults handles partial signing results."""
    def catChan2Success(url):
      if url.startswith('chan2'):
        result = mock.Mock()
        result.output = self.SIGNER_RESULT
        return result
      else:
        raise release_stages.gs.GSNoSuchKey()

    with patch(release_stages.gs, 'GSContext') as mock_gs_ctx_init:
      mock_gs_ctx = mock_gs_ctx_init.return_value
      mock_gs_ctx.Cat.side_effect = catChan2Success
      notifier = mock.Mock()

      stage = self.ConstructStage()
      self.assertFalse(
          stage._CheckForResults(mock_gs_ctx,
                                 self.INSNS_URLS_PER_CHANNEL,
                                 notifier))
      self.assertEqual(stage.signing_results, {
          'chan1': {},
          'chan2': {
              'chan2_uri1.json': {
                  'board': 'link',
                  'channel': 'stable',
                  'keyset': 'link-mp-v4',
                  'status': {'status': 'passed'},
                  'type': 'recovery'
              }
          }
      })
      self.assertEqual(notifier.mock_calls, [mock.call('chan2')])

  def testCheckForResultsUnexpectedJson(self):
    """Verify _CheckForResults handles unexpected Json values."""
    with patch(release_stages.gs, 'GSContext') as mock_gs_ctx_init:
      mock_gs_ctx = mock_gs_ctx_init.return_value
      mock_gs_ctx.Cat.return_value.output = "{}"
      notifier = mock.Mock()

      stage = self.ConstructStage()
      self.assertFalse(
          stage._CheckForResults(mock_gs_ctx,
                                 self.INSNS_URLS_PER_CHANNEL,
                                 notifier))
      self.assertEqual(stage.signing_results, {
          'chan1': {}, 'chan2': {}
      })
      self.assertEqual(notifier.mock_calls, [])

  def testCheckForResultsNoResult(self):
    """Verify _CheckForResults handles missing signer results."""
    with patch(release_stages.gs, 'GSContext') as mock_gs_ctx_init:
      mock_gs_ctx = mock_gs_ctx_init.return_value
      mock_gs_ctx.Cat.side_effect = release_stages.gs.GSNoSuchKey
      notifier = mock.Mock()

      stage = self.ConstructStage()
      self.assertFalse(
          stage._CheckForResults(mock_gs_ctx,
                                 self.INSNS_URLS_PER_CHANNEL,
                                 notifier))
      self.assertEqual(stage.signing_results, {
          'chan1': {}, 'chan2': {}
      })
      self.assertEqual(notifier.mock_calls, [])

  def testCheckForResultsFailed(self):
    """Verify _CheckForResults handles missing signer results."""
    with patch(release_stages.gs, 'GSContext') as mock_gs_ctx_init:
      mock_gs_ctx = mock_gs_ctx_init.return_value
      mock_gs_ctx.Cat.side_effect = release_stages.gs.GSNoSuchKey
      notifier = mock.Mock()

      stage = self.ConstructStage()
      self.assertFalse(
          stage._CheckForResults(mock_gs_ctx,
                                 self.INSNS_URLS_PER_CHANNEL,
                                 notifier))
      self.assertEqual(stage.signing_results, {
          'chan1': {}, 'chan2': {}
      })
      self.assertEqual(notifier.mock_calls, [])

  def generateNotifyCalls(self, channels):
    def side_effect(_, notifier):
      for channel in channels:
        notifier(channel)
    return side_effect

  @unittest.skipIf(not CROSTOOLS_AVAILABLE,
                   'Internal crostools repository needed.')
  def testPerformStageSuccess(self):
    """Test that SignerResultsStage works when signing works."""

    with patch(release_stages.parallel, 'BackgroundTaskRunner') as background:
      queue = background().__enter__()

      stage = self.ConstructStage()

      with patch(stage, '_WaitForPushImage') as wait_push:
        with patch(stage, '_WaitForSigningResults') as wait_signing:
          wait_push.return_value = self.INSNS_URLS_PER_CHANNEL
          wait_signing.side_effect = self.generateNotifyCalls(('stable',
                                                               'beta'))
          stage.PerformStage()

      # Verify that we queue up work
      queue.put.assert_any_call(('stable', 'x86-mario', '0.0.1', False, True))
      queue.put.assert_any_call(('beta', 'x86-mario', '0.0.1', False, True))


  @unittest.skipIf(not CROSTOOLS_AVAILABLE,
                   'Internal crostools repository needed.')
  def testPerformStageSuccessVarientBoard(self):
    """Test that SignerResultsStage works with varient boards.

    Varient boards need some name conversion. Make sure that's okay.
    """
    self._current_board = 'x86-alex_he'

    with patch(release_stages.parallel, 'BackgroundTaskRunner') as background:
      queue = background().__enter__()

      stage = self.ConstructStage()

      with patch(stage, '_WaitForPushImage') as wait_push:
        with patch(stage, '_WaitForSigningResults') as wait_signing:
          wait_push.return_value = self.INSNS_URLS_PER_CHANNEL
          wait_signing.side_effect = self.generateNotifyCalls(('stable',
                                                               'beta'))
          stage.PerformStage()

      # Verify that we queue up work
      queue.put.assert_any_call(('stable', 'x86-alex-he', '0.0.1', False, True))
      queue.put.assert_any_call(('beta', 'x86-alex-he', '0.0.1', False, True))

  @unittest.skipIf(not CROSTOOLS_AVAILABLE,
                   'Internal crostools repository needed.')
  def testPerformStageSigningFailed(self):
    """Test that SignerResultsStage works when signing works."""
    with patch(release_stages.parallel, 'BackgroundTaskRunner') as background:
      queue = background().__enter__()

      stage = self.ConstructStage()

      with patch(stage, '_WaitForPushImage') as wait_push:
        with patch(stage, '_WaitForSigningResults') as wait_signing:
          wait_push.return_value = self.INSNS_URLS_PER_CHANNEL
          wait_signing.side_effect = release_stages.SignerFailure

          self.assertRaises(release_stages.SignerFailure,
                            stage.PerformStage)

      # Ensure no work was queued up.
      self.assertFalse(queue.put.called)

  @unittest.skipIf(not CROSTOOLS_AVAILABLE,
                   'Internal crostools repository needed.')
  def testPerformStageTrybot(self):
    """Test the PerformStage alternate behavior for trybot runs."""
    with patch(release_stages.parallel, 'BackgroundTaskRunner') as background:
      queue = background().__enter__()

      # The stage is constructed differently for trybots, so don't use
      # ConstructStage.
      stage = release_stages.PaygenStage(self._run, self._current_board,
                                 archive_stage=None, channels=['foo', 'bar'])
      with patch(stage, '_WaitForPushImage') as wait_push:
        with patch(stage, '_WaitForSigningResults') as wait_signing:
          stage.PerformStage()

        # Make sure we don't wait on push_image or signing in this case.
        self.assertEqual(wait_push.mock_calls, [])
        self.assertEqual(wait_signing.mock_calls, [])

      # Notice that we didn't put anything in _wait_for_channel_signing, but
      # still got results right away.
      queue.put.assert_any_call(('foo', 'x86-mario', '0.0.1', False, True))
      queue.put.assert_any_call(('bar', 'x86-mario', '0.0.1', False, True))

  @unittest.skipIf(not CROSTOOLS_AVAILABLE,
                   'Internal crostools repository needed.')
  def testPerformStageUnknownBoard(self):
    """Test that SignerResultsStage works when signing works."""
    self._current_board = 'unknown-board-name'
    stage = self.ConstructStage()

    self.assertRaises(release_stages.PaygenNoPaygenConfigForBoard,
                      stage.PerformStage)

  @unittest.skipIf(not CROSTOOLS_AVAILABLE,
                   'Internal crostools repository needed.')
  def testRunPaygenInProcess(self):
    """Test that SignerResultsStage works when signing works."""
    with patch(paygen_build_lib, 'CreatePayloads') as create_payloads:
      # Call the method under test.
      stage = self.ConstructStage()
      stage._RunPaygenInProcess('foo', 'foo-board', 'foo-version', False, True)

      # Ensure arguments are properly converted and passed along.
      create_payloads.assert_called_with(gspaths.Build(version='foo-version',
                                                       board='foo-board',
                                                       channel='foo-channel'),
                                         dry_run=False,
                                         work_dir=mock.ANY,
                                         run_parallel=True,
                                         run_on_builder=True,
                                         skip_test_payloads=False,
                                         skip_autotest=False)

  @unittest.skipIf(not CROSTOOLS_AVAILABLE,
                   'Internal crostools repository needed.')
  def testRunPaygenInProcessComplex(self):
    """Test that SignerResultsStage with arguments that are more unusual."""
    with patch(paygen_build_lib, 'CreatePayloads') as create_payloads:
      # Call the method under test.
      # Use release tools channel naming, and a board name including a variant.
      stage = self.ConstructStage()
      stage._RunPaygenInProcess('foo-channel', 'foo-board-variant',
                                'foo-version', True, False)

      # Ensure arguments are properly converted and passed along.
      create_payloads.assert_called_with(
          gspaths.Build(version='foo-version',
                        board='foo-board-variant',
                        channel='foo-channel'),
          dry_run=True,
          work_dir=mock.ANY,
          run_parallel=True,
          run_on_builder=True,
          skip_test_payloads=True,
          skip_autotest=True)


if __name__ == '__main__':
  cros_test_lib.main()
