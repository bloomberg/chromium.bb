# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the various stages that a builder runs."""

from __future__ import print_function

import json
import os

from chromite.cbuildbot import commands
from chromite.cbuildbot import failures_lib
from chromite.cbuildbot.stages import artifact_stages
from chromite.lib import cros_logging as logging
from chromite.lib import gs
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import timeout_util
from chromite.lib.paygen import gspaths
from chromite.lib.paygen import paygen_build_lib


class InvalidTestConditionException(Exception):
  """Raised when pre-conditions for a test aren't met."""


class SignerTestStage(artifact_stages.ArchivingStage):
  """Run signer related tests."""

  option_name = 'tests'
  config_name = 'signer_tests'

  # If the signer tests take longer than 30 minutes, abort. They usually take
  # five minutes to run.
  SIGNER_TEST_TIMEOUT = 30 * 60

  def PerformStage(self):
    if not self.archive_stage.WaitForRecoveryImage():
      raise InvalidTestConditionException('Missing recovery image.')
    with timeout_util.Timeout(self.SIGNER_TEST_TIMEOUT):
      commands.RunSignerTests(self._build_root, self._current_board)


class SignerResultsTimeout(failures_lib.StepFailure):
  """The signer did not produce any results inside the expected time."""


class SignerFailure(failures_lib.StepFailure):
  """The signer returned an error result."""


class MissingInstructionException(failures_lib.StepFailure):
  """We didn't receive the list of signing instructions PushImage uploaded."""


class MalformedResultsException(failures_lib.StepFailure):
  """The Signer results aren't formatted as we expect."""


class PaygenSigningRequirementsError(failures_lib.StepFailure):
  """Paygen stage can't run if signing failed."""


class PaygenCrostoolsNotAvailableError(failures_lib.StepFailure):
  """Paygen stage can't run if signing failed."""


class PaygenNoPaygenConfigForBoard(failures_lib.StepFailure):
  """Paygen can't run with a release.conf config for the board."""


class PaygenStage(artifact_stages.ArchivingStage):
  """Stage that generates release payloads.

  If this stage is created with a 'channels' argument, it can run
  independently. Otherwise, it's dependent on values queued up by
  the ArchiveStage (push_image).
  """
  option_name = 'paygen'
  config_name = 'paygen'

  # Poll for new results every 30 seconds.
  SIGNING_PERIOD = 30

  # Timeout for the signing process. 2 hours in seconds.
  SIGNING_TIMEOUT = 2 * 60 * 60

  FINISHED = 'finished'

  def __init__(self, builder_run, board, archive_stage, channels=None,
               **kwargs):
    """Init that accepts the channels argument, if present.

    Args:
      builder_run: See builder_run on ArchivingStage.
      board: See board on ArchivingStage.
      archive_stage: See archive_stage on ArchivingStage.
      channels: Explicit list of channels to generate payloads for.
                If empty, will instead wait on values from push_image.
                Channels is normally None in release builds, and normally set
                for trybot 'payloads' builds.
    """
    super(PaygenStage, self).__init__(builder_run, board, archive_stage,
                                      **kwargs)
    self.signing_results = {}
    self.channels = channels

  def _HandleStageException(self, exc_info):
    """Override and don't set status to FAIL but FORGIVEN instead."""
    exc_type, exc_value, _exc_tb = exc_info

    # If Paygen fails to find anything needed in release.conf, treat it
    # as a warning, not a failure. This is common during new board bring up.
    if issubclass(exc_type, PaygenNoPaygenConfigForBoard):
      return self._HandleExceptionAsWarning(exc_info)

    # Warn so people look at ArchiveStage for the real error.
    if issubclass(exc_type, MissingInstructionException):
      return self._HandleExceptionAsWarning(exc_info)

    # If the exception is a TestLabFailure that means we couldn't schedule the
    # test. We don't fail the build for that. We do the CompoundFailure dance,
    # because that's how we'll get failures from background processes returned
    # to us.
    if (issubclass(exc_type, failures_lib.TestLabFailure) or
        (issubclass(exc_type, failures_lib.CompoundFailure) and
         exc_value.MatchesFailureType(failures_lib.TestLabFailure))):
      return self._HandleExceptionAsWarning(exc_info)

    return super(PaygenStage, self)._HandleStageException(exc_info)

  def _JsonFromUrl(self, gs_ctx, url):
    """Fetch a GS Url, and parse it as Json.

    Args:
      gs_ctx: GS Context.
      url: Url to fetch and parse.

    Returns:
      None if the Url doesn't exist.
      Parsed Json structure if it did.

    Raises:
      MalformedResultsException if it failed to parse.
    """
    try:
      signer_txt = gs_ctx.Cat(url)
    except gs.GSNoSuchKey:
      return None

    try:
      return json.loads(signer_txt)
    except ValueError:
      # We should never see malformed Json, even for intermediate statuses.
      raise MalformedResultsException(signer_txt)

  def _SigningStatusFromJson(self, signer_json):
    """Extract a signing status from a signer result Json DOM.

    Args:
      signer_json: The parsed json status from a signer operation.

    Returns:
      string with a simple status: 'passed', 'failed', 'downloading', etc,
      or '' if the json doesn't contain a status.
    """
    return (signer_json or {}).get('status', {}).get('status', '')

  def _CheckForResults(self, gs_ctx, instruction_urls_per_channel,
                       channel_notifier):
    """timeout_util.WaitForSuccess func to check a list of signer results.

    Args:
      gs_ctx: Google Storage Context.
      instruction_urls_per_channel: Urls of the signer result files
                                    we're expecting.
      channel_notifier: BackgroundTaskRunner into which we push channels for
                        processing.

    Returns:
      Number of results not yet collected.
    """
    COMPLETED_STATUS = ('passed', 'failed')

    # Assume we are done, then try to prove otherwise.
    results_completed = True

    for channel in instruction_urls_per_channel.keys():
      self.signing_results.setdefault(channel, {})

      if (len(self.signing_results[channel]) ==
          len(instruction_urls_per_channel[channel])):
        continue

      for url in instruction_urls_per_channel[channel]:
        # Convert from instructions URL to instructions result URL.
        url += '.json'

        # We already have a result for this URL.
        if url in self.signing_results[channel]:
          continue

        try:
          signer_json = self._JsonFromUrl(gs_ctx, url)
        except MalformedResultsException as e:
          logging.warning('Received malformed json: %s', e)
          continue

        if self._SigningStatusFromJson(signer_json) in COMPLETED_STATUS:
          # If we find a completed result, remember it.
          self.signing_results[channel][url] = signer_json

      # If we don't have full results for this channel, we aren't done
      # waiting.
      if (len(self.signing_results[channel]) !=
          len(instruction_urls_per_channel[channel])):
        results_completed = False
        continue

      # If we reach here, the channel has just been completed for the first
      # time.

      # If all results 'passed' the channel was successfully signed.
      channel_success = True
      for signer_result in self.signing_results[channel].values():
        if self._SigningStatusFromJson(signer_result) != 'passed':
          channel_success = False

      # If we successfully completed the channel, inform paygen.
      if channel_success:
        channel_notifier(channel)

    return results_completed

  def _WaitForPushImage(self):
    """Block until push_image data is ready.

    Returns:
      Push_image results, expected to be of the form:
      { 'channel': ['gs://instruction_uri1', 'gs://signer_instruction_uri2'] }

    Raises:
      MissingInstructionException: If push_image sent us an error, or timed out.
    """
    # This call will NEVER time out.
    instruction_urls_per_channel = self.board_runattrs.GetParallel(
        'instruction_urls_per_channel', timeout=None)

    # A value of None signals an error in PushImage.
    if instruction_urls_per_channel is None:
      raise MissingInstructionException(
          'ArchiveStage: PushImage failed. No images means no Paygen.')

    return instruction_urls_per_channel

  def _WaitForSigningResults(self,
                             instruction_urls_per_channel,
                             channel_notifier):
    """Do the work of waiting for signer results and logging them.

    Args:
      instruction_urls_per_channel: push_image data (see _WaitForPushImage).
      channel_notifier: BackgroundTaskRunner into which we push channels for
                        processing.

    Raises:
      ValueError: If the signer result isn't valid json.
      RunCommandError: If we are unable to download signer results.
    """
    gs_ctx = gs.GSContext(dry_run=self._run.debug)

    try:
      logging.info('Waiting for signer results.')
      timeout_util.WaitForReturnTrue(
          self._CheckForResults,
          func_args=(gs_ctx, instruction_urls_per_channel, channel_notifier),
          timeout=self.SIGNING_TIMEOUT, period=self.SIGNING_PERIOD)
    except timeout_util.TimeoutError:
      msg = 'Image signing timed out.'
      logging.error(msg)
      logging.PrintBuildbotStepText(msg)
      raise SignerResultsTimeout(msg)

    # Log all signer results, then handle any signing failures.
    failures = []
    for url_results in self.signing_results.values():
      for url, signer_result in url_results.iteritems():
        result_description = os.path.basename(url)
        logging.PrintBuildbotStepText(result_description)
        logging.info('Received results for: %s', result_description)
        logging.info(json.dumps(signer_result, indent=4))

        status = self._SigningStatusFromJson(signer_result)
        if status != 'passed':
          failures.append(result_description)
          logging.error('Signing failed for: %s', result_description)

    if failures:
      logging.error('Failure summary:')
      for failure in failures:
        logging.error('  %s', failure)
      raise SignerFailure(', '.join([str(f) for f in failures]))

  def PerformStage(self):
    """Do the work of generating our release payloads."""
    # Convert to release tools naming for boards.
    board = self._current_board.replace('_', '-')
    version = self._run.attrs.release_tag

    assert version, "We can't generate payloads without a release_tag."
    logging.info("Generating payloads for: %s, %s", board, version)

    # Test to see if the current board has a Paygen configuration. We do
    # this here, no in the sub-process so we don't have to pass back a
    # failure reason.
    try:
      paygen_build_lib.ValidateBoardConfig(board)
    except  paygen_build_lib.BoardNotConfigured:
      raise PaygenNoPaygenConfigForBoard(
          'No release.conf entry was found for board %s. Get a TPM to fix.' %
          board)

    with parallel.BackgroundTaskRunner(self._RunPaygenInProcess) as per_channel:
      def channel_notifier(channel):
        per_channel.put((channel, board, version, self._run.debug,
                         self._run.config.paygen_skip_testing,
                         self._run.config.paygen_skip_delta_payloads))

      if self.channels:
        logging.info("Using explicit channels: %s", self.channels)
        # If we have an explicit list of channels, use it.
        for channel in self.channels:
          channel_notifier(channel)
      else:
        instruction_urls_per_channel = self._WaitForPushImage()
        self._WaitForSigningResults(instruction_urls_per_channel,
                                    channel_notifier)

  def _RunPaygenInProcess(self, channel, board, version, debug,
                          disable_tests, skip_delta_payloads):
    """Helper for PaygenStage that invokes payload generation.

    This method is intended to be safe to invoke inside a process.

    Args:
      channel: Channel of payloads to generate ('stable', 'beta', etc)
      board: Board of payloads to generate ('x86-mario', 'x86-alex-he', etc)
      version: Version of payloads to generate.
      debug: Flag telling if this is a real run, or a test run.
      disable_tests: Do not generate test artifacts are run payload tests.
      skip_delta_payloads: Skip generating delta payloads.
    """
    # Convert to release tools naming for channels.
    if not channel.endswith('-channel'):
      channel += '-channel'

    with osutils.TempDir(sudo_rm=True) as tempdir:
      # Create the definition of the build to generate payloads for.
      build = gspaths.Build(channel=channel,
                            board=board,
                            version=version)

      try:
        # Generate the payloads.
        self._PrintLoudly('Starting %s, %s, %s' % (channel, version, board))
        paygen_build_lib.CreatePayloads(build,
                                        work_dir=tempdir,
                                        site_config=self._run.site_config,
                                        dry_run=debug,
                                        run_parallel=True,
                                        run_on_builder=True,
                                        skip_delta_payloads=skip_delta_payloads,
                                        disable_tests=disable_tests)
      except (paygen_build_lib.BuildFinished,
              paygen_build_lib.BuildLocked,
              paygen_build_lib.BuildSkip) as e:
        # These errors are normal if it's possible for another process to
        # work on the same build. This process could be a Paygen server, or
        # another builder (perhaps by a trybot generating payloads on request).
        #
        # This means the build was finished by the other process, is already
        # being processed (so the build is locked), or that it's been marked
        # to skip (probably done manually).
        logging.info('Paygen skipped because: %s', e)
