#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for build stages."""

import sys
import unittest

import constants
sys.path.insert(0, constants.SOURCE_ROOT)
from chromite.buildbot import cbuildbot_stages as stages
from chromite.buildbot.stages import artifact_stages
from chromite.buildbot.stages import generic_stages_unittest
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
class SignerResultsStageTest(generic_stages_unittest.AbstractStageTest):
  """Test the SignerResultsStage."""

  BOT_ID = 'x86-mario-release'
  RELEASE_TAG = '0.0.1'

  def setUp(self):
    self.StartPatcher(BuilderRunMock())
    self._Prepare()

    self.signer_result = """
      { "status": { "status": "passed" }, "board": "link",
      "keyset": "link-mp-v4", "type": "recovery", "channel": "stable" }
      """

    self.insns_urls_per_channel = {
        'chan1': ['chan1_uri1', 'chan1_uri2'],
        'chan2': ['chan2_uri1'],
    }


  def ConstructStage(self):
    archive_stage = artifact_stages.ArchiveStage(self._run, self._current_board)
    stage = stages.SignerResultsStage(
        self._run, self._current_board, archive_stage)

    return stage

  def testPerformStageSuccess(self):
    """Test that SignerResultsStage works when signing works."""
    results = ['chan1_uri1.json', 'chan1_uri2.json', 'chan2_uri1.json']

    with patch(stages.gs, 'GSContext') as mock_gs_ctx_init:
      mock_gs_ctx = mock_gs_ctx_init.return_value
      mock_gs_ctx.Cat.return_value.output = self.signer_result

      stage = self.ConstructStage()
      stage.board_runattrs.SetParallel(
          'instruction_urls_per_channel', self.insns_urls_per_channel)

      stage.PerformStage()
      for result in results:
        mock_gs_ctx.Cat.assert_any_call(result)

      self.assertEqual(stage.archive_stage.WaitForChannelSigning(), 'chan1')
      self.assertEqual(stage.archive_stage.WaitForChannelSigning(), 'chan2')
      self.assertEqual(stage.archive_stage.WaitForChannelSigning(),
                       stages.SignerResultsStage.FINISHED)

  def testPerformStageSuccessNothingSigned(self):
    """Test that SignerResultsStage passes when there are no signed images."""
    with patch(stages.gs, 'GSContext') as mock_gs_ctx_init:
      mock_gs_ctx = mock_gs_ctx_init.return_value
      mock_gs_ctx.Cat.return_value.output = self.signer_result

      stage = self.ConstructStage()
      stage.board_runattrs.SetParallel('instruction_urls_per_channel', {})

      stage.PerformStage()
      self.assertFalse(mock_gs_ctx.Cat.called)
      self.assertEqual(stage.archive_stage.WaitForChannelSigning(),
                       stages.SignerResultsStage.FINISHED)

  def testPerformStageInstructionUrlsTimeout(self):
    """Test that SignerResultsStage passes when there are no signed images."""
    with patch(stages.gs, 'GSContext') as mock_gs_ctx_init:
      mock_gs_ctx = mock_gs_ctx_init.return_value
      mock_gs_ctx.Cat.return_value.output = self.signer_result

      stage = self.ConstructStage()

      # Shorten the timeout so the tests finish in a reasonable period of time.
      stage.PUSHIMAGE_TIMEOUT = 0.01

      self.assertRaises(stages.MissingInstructionException, stage.PerformStage)
      self.assertFalse(mock_gs_ctx.Cat.called)

  def testPerformStageFailure(self):
    """Test that SignerResultsStage errors when the signers report an error."""
    with patch(stages.gs, 'GSContext') as mock_gs_ctx_init:
      mock_gs_ctx = mock_gs_ctx_init.return_value
      mock_gs_ctx.Cat.return_value.output = """
          { "status": { "status": "failed" }, "board": "link",
            "keyset": "link-mp-v4", "type": "recovery", "channel": "stable" }
          """
      stage = self.ConstructStage()
      stage.board_runattrs.SetParallel('instruction_urls_per_channel',
          {'chan1': ['chan1_uri1']})
      self.assertRaises(stages.SignerFailure, stage.PerformStage)

  def testPerformStageMalformedJson(self):
    """Test that SignerResultsStage errors when invalid Json is received.."""
    with patch(stages.gs, 'GSContext') as mock_gs_ctx_init:
      mock_gs_ctx = mock_gs_ctx_init.return_value
      mock_gs_ctx.Cat.return_value.output = "{"

      stage = self.ConstructStage()
      stage.board_runattrs.SetParallel('instruction_urls_per_channel',
          {'chan1': ['chan1_uri1']})

      self.assertRaises(stages.MalformedResultsException, stage.PerformStage)

  def testPerformStageTimeout(self):
    """Test that SignerResultsStage reports timeouts correctly."""
    with patch(stages.timeout_util, 'WaitForSuccess') as mock_wait:
      mock_wait.side_effect = timeout_util.TimeoutError

      stage = self.ConstructStage()
      stage.board_runattrs.SetParallel('instruction_urls_per_channel',
          {'chan1': ['chan1_uri1']})
      self.assertRaises(stages.SignerResultsTimeout, stage.PerformStage)

  def testCheckForResultsSuccess(self):
    """Test that SignerResultsStage works when signing works."""
    with patch(stages.gs, 'GSContext') as mock_gs_ctx_init:
      mock_gs_ctx = mock_gs_ctx_init.return_value
      mock_gs_ctx.Cat.return_value.output = self.signer_result

      stage = self.ConstructStage()
      self.assertTrue(
          stage._CheckForResults(mock_gs_ctx,
          self.insns_urls_per_channel))

  def testCheckForResultsSuccessNoChannels(self):
    """Test that SignerResultsStage works when there is nothing to check for."""
    with patch(stages.gs, 'GSContext') as mock_gs_ctx_init:
      mock_gs_ctx = mock_gs_ctx_init.return_value

      stage = self.ConstructStage()

      # Ensure we find that we are ready if there are no channels to look for.
      self.assertTrue(stage._CheckForResults(mock_gs_ctx, {}))

      # Ensure we didn't contact GS while checking for no channels.
      self.assertFalse(mock_gs_ctx.Cat.called)

  def testCheckForResultsPartialComplete(self):
    """Verify _CheckForResults handles partial signing results."""
    def catChan2Success(url):
      if url.startswith('chan2'):
        result = mock.Mock()
        result.output = self.signer_result
        return result
      else:
        raise stages.gs.GSNoSuchKey()

    with patch(stages.gs, 'GSContext') as mock_gs_ctx_init:
      mock_gs_ctx = mock_gs_ctx_init.return_value
      mock_gs_ctx.Cat.side_effect = catChan2Success

      stage = self.ConstructStage()
      self.assertFalse(
          stage._CheckForResults(mock_gs_ctx,
          self.insns_urls_per_channel))
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

  def testCheckForResultsUnexpectedJson(self):
    """Verify _CheckForResults handles unexpected Json values."""
    with patch(stages.gs, 'GSContext') as mock_gs_ctx_init:
      mock_gs_ctx = mock_gs_ctx_init.return_value
      mock_gs_ctx.Cat.return_value.output = "{}"

      stage = self.ConstructStage()
      self.assertFalse(
          stage._CheckForResults(mock_gs_ctx,
          self.insns_urls_per_channel))
      self.assertEqual(stage.signing_results, {
          'chan1': {}, 'chan2': {}
      })

  def testCheckForResultsNoResult(self):
    """Verify _CheckForResults handles missing signer results."""
    with patch(stages.gs, 'GSContext') as mock_gs_ctx_init:
      mock_gs_ctx = mock_gs_ctx_init.return_value
      mock_gs_ctx.Cat.side_effect = stages.gs.GSNoSuchKey

      stage = self.ConstructStage()
      self.assertFalse(
          stage._CheckForResults(mock_gs_ctx,
          self.insns_urls_per_channel))
      self.assertEqual(stage.signing_results, {
          'chan1': {}, 'chan2': {}
      })


class PaygenStageTest(generic_stages_unittest.StageTest):
  """Test the PaygenStageStage."""

  BOT_ID = 'x86-mario-release'
  RELEASE_TAG = '0.0.1'

  def setUp(self):
    self.StartPatcher(BuilderRunMock())
    self._Prepare()

  def ConstructStage(self):
    archive_stage = artifact_stages.ArchiveStage(self._run, self._current_board)
    return stages.PaygenStage(self._run, self._current_board, archive_stage)

  def testPerformStageSuccess(self):
    """Test that SignerResultsStage works when signing works."""

    with patch(stages.parallel, 'BackgroundTaskRunner') as background:
      queue = background().__enter__()

      stage = self.ConstructStage()
      stage.archive_stage.AnnounceChannelSigned('stable')
      stage.archive_stage.AnnounceChannelSigned('beta')
      stage.archive_stage.AnnounceChannelSigned(
          stages.SignerResultsStage.FINISHED)
      stage.PerformStage()

      # Verify that we queue up work
      queue.put.assert_any_call(('stable', 'x86-mario', '0.0.1', False, True))
      queue.put.assert_any_call(('beta', 'x86-mario', '0.0.1', False, True))

  def testPerformStageNoChannels(self):
    """Test that SignerResultsStage works when signing works."""
    with patch(stages.parallel, 'BackgroundTaskRunner') as background:
      queue = background().__enter__()

      stage = self.ConstructStage()
      stage.archive_stage.AnnounceChannelSigned(
          stages.SignerResultsStage.FINISHED)
      stage.PerformStage()

      # Ensure no work was queued up.
      self.assertFalse(queue.put.called)

  def testPerformSigningFailed(self):
    """Test that SignerResultsStage works when signing works."""
    with patch(stages.parallel, 'BackgroundTaskRunner') as background:
      queue = background().__enter__()

      stage = self.ConstructStage()
      stage.archive_stage.AnnounceChannelSigned(None)

      self.assertRaises(stages.PaygenSigningRequirementsError,
                        stage.PerformStage)

      # Ensure no work was queued up.
      self.assertFalse(queue.put.called)

  def testPerformTrybot(self):
    """Test the PerformStage alternate behavior for trybot runs."""
    with patch(stages.parallel, 'BackgroundTaskRunner') as background:
      queue = background().__enter__()

      # The stage is constructed differently for trybots, so don't use
      # ConstructStage.
      stage = stages.PaygenStage(self._run, self._current_board,
                                 archive_stage=None, channels=['foo', 'bar'])
      stage.PerformStage()

      # Notice that we didn't put anything in _wait_for_channel_signing, but
      # still got results right away.
      queue.put.assert_any_call(('foo', 'x86-mario', '0.0.1', False, True))
      queue.put.assert_any_call(('bar', 'x86-mario', '0.0.1', False, True))

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
      stage._RunPaygenInProcess('foo-channel', 'foo-board_variant',
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
