# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""PayGen - Automatic Payload Generation.

This library processes a single build at a time, and decides which payloads
need to be generated. It then calls paygen_payload to generate each payload.

This library is reponsible for locking builds during processing, and checking
and setting flags to show that a build has been processed.
"""

from __future__ import print_function

import json
import operator
import os
import shutil
import socket
import sys
import tempfile
import urlparse

from chromite.cbuildbot import commands
from chromite.cbuildbot import constants
from chromite.cbuildbot import config_lib
from chromite.cbuildbot import failures_lib
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import parallel
from chromite.lib import retry_util
from chromite.lib.paygen import download_cache
from chromite.lib.paygen import dryrun_lib
from chromite.lib.paygen import gslib
from chromite.lib.paygen import gslock
from chromite.lib.paygen import gspaths
from chromite.lib.paygen import paygen_payload_lib
from chromite.lib.paygen import urilib


# For crostools access.
sys.path.insert(0, constants.SOURCE_ROOT)

AUTOTEST_DIR = os.path.join(constants.SOURCE_ROOT, 'src', 'third_party',
                            'autotest', 'files')
sys.path.insert(0, AUTOTEST_DIR)

# If we are an external only checkout, or a bootstrap environemnt these imports
# will fail. We quietly ignore the failure, but leave bombs around that will
# explode if people try to really use this library.
try:
  # pylint: disable=F0401
  from site_utils.autoupdate.lib import test_params
  from site_utils.autoupdate.lib import test_control
  # pylint: enable=F0401

except ImportError:
  test_params = None
  test_control = None


# The oldest release milestone for which run_suite should be attempted.
RUN_SUITE_MIN_MSTONE = 30

# Used to format timestamps on archived paygen.log file names in GS.
PAYGEN_LOG_TIMESTAMP_FORMAT = '%Y%m%d-%H%M%S-UTC'

# Board and device information published by goldeneye.
BOARDS_URI = 'gs://chromeos-build-release-console/boards.json'
FSI_URI = 'gs://chromeos-build-release-console/fsis.json'
OMAHA_URI = 'gs://chromeos-build-release-console/omaha_status.json'


class Error(Exception):
  """Exception base class for this module."""


class EarlyExit(Error):
  """Base class for paygen_build 'normal' errors.

  There are a number of cases in which a paygen run fails for reasons that
  require special reporting, but which are normal enough to avoid raising
  big alarms. We signal these results using exceptions derived from this
  class.

  Note that the docs strings on the subclasses may be displayed directly
  to the user, and RESULT may be returned as an exit code.
  """

  def __str__(self):
    """Return the doc string to the user as the exception description."""
    return self.__doc__


class BuildFinished(EarlyExit):
  """This build has already been marked as finished, no need to process."""
  RESULT = 22


class BuildLocked(EarlyExit):
  """This build is locked and already being processed elsewhere."""
  RESULT = 23


class BuildSkip(EarlyExit):
  """This build has been marked as skip, and should not be processed."""
  RESULT = 24


class BuildNotReady(EarlyExit):
  """Not all images for this build are uploaded, don't process it yet."""
  RESULT = 25


class BoardNotConfigured(EarlyExit):
  """The board does not exist in the published goldeneye records."""
  RESULT = 26


class BuildCorrupt(Error):
  """Exception raised if a build has unexpected images."""


class ImageMissing(Error):
  """Exception raised if a build doesn't have expected images."""


class PayloadTestError(Error):
  """Raised when an error is encountered with generation of test artifacts."""


class ArchiveError(Error):
  """Raised when there was a failure to map a build to the images archive."""


def _LogList(title, obj_list):
  """Helper for logging a list of objects.

  Generates:
    1: ObjA
    2: ObjB
    3: ObjC
    ...

  Args:
    title: Title string for the list.
    obj_list: List of objects to convert to string and log, one per line.
  """
  logging.info('%s:', title)

  if not obj_list:
    logging.info(' (no objects listed)')
    return

  index = 0

  for obj in obj_list:
    index += 1
    logging.info(' %2d: %s', index, obj)


def _FilterForImages(artifacts):
  """Return only instances of Image from a list of artifacts."""
  return filter(gspaths.IsImage, artifacts)


def _FilterForMp(artifacts):
  """Return the MP keyed images in a list of artifacts.

  This returns all images with key names of the form "mp", "mp-v3", etc.

  Args:
    artifacts: The list of artifacts to filter.

  Returns:
    List of MP images.
  """
  return [i for i in _FilterForImages(artifacts) if 'mp' in i.key.split('-')]


def _FilterForPremp(artifacts):
  """Return the PreMp keyed images in a list of artifacts.

  The key for an images is expected to be of the form "premp", "mp", or
  "mp-vX". This filter returns everything that is "premp".

  Args:
    artifacts: The list of artifacts to filter.

  Returns:
    List of PreMP images.
  """
  return [i for i in _FilterForImages(artifacts) if 'premp' in i.key.split('-')]


def _FilterForBasic(artifacts):
  """Return the basic (not NPO) images in a list of artifacts.

  As an example, an image for a stable channel build might be in the
  "stable-channel", or it might be in the "npo-channel". This only returns
  the basic images that match "stable-channel".

  Args:
    artifacts: The list of artifacts to filter.

  Returns:
    List of basic images.
  """
  return [i for i in _FilterForImages(artifacts) if i.image_channel is None]


def _FilterForNpo(artifacts):
  """Return the NPO images in a list of artifacts.

  Return the N Plus One images in the given list.

  Args:
    artifacts: The list of artifacts to filter.

  Returns:
    List of NPO images.
  """
  return [i for i in _FilterForImages(artifacts)
          if i.image_channel == 'nplusone-channel']


def _FilterForUnsignedImageArchives(artifacts):
  """Return only instances of UnsignedImageArchive from a list of artifacts."""
  return filter(gspaths.IsUnsignedImageArchive, artifacts)


def _FilterForImageType(artifacts, image_type):
  """Return only images for given |image_type|."""
  return [i for i in artifacts if i.image_type == image_type]


def _FilterForValidImageType(artifacts):
  """Return only images with image types that paygen supports."""
  v = gspaths.ChromeosReleases.UNSIGNED_IMAGE_TYPES
  return reduce(operator.add, [_FilterForImageType(artifacts, x) for x in v])


def _FilterForTest(artifacts):
  """Return only test images archives."""
  return [i for i in _FilterForUnsignedImageArchives(artifacts)
          if i.image_type == 'test']


def _GenerateSinglePayload(payload, work_dir, sign, au_generator_uri, dry_run):
  """Generate a single payload.

  This is intended to be safe to call inside a new process.

  Args:
    payload: gspath.Payload object defining the payloads to generate.
    work_dir: Working directory for payload generation.
    sign: boolean to decide if payload should be signed.
    au_generator_uri: URI of the au_generator.zip to use, None for the default.
    dry_run: boolean saying if this is a dry run.
  """
  # This cache dir will be shared with other processes, but we need our
  # own instance of the cache manager to properly coordinate.
  cache_dir = paygen_payload_lib.FindCacheDir()
  with download_cache.DownloadCache(
      cache_dir, cache_size=_PaygenBuild.CACHE_SIZE) as cache:
    # Actually generate the payload.
    paygen_payload_lib.CreateAndUploadPayload(
        payload,
        cache,
        work_dir=work_dir,
        sign=sign,
        au_generator_uri=au_generator_uri,
        dry_run=dry_run)


class _PaygenBuild(object):
  """This class is responsible for generating the payloads for a given build.

  It operates across a single build at a time, and is responsible for locking
  that build and for flagging it as finished when all payloads are generated.
  """
  # 50 GB of cache.
  CACHE_SIZE = 50 * 1024 * 1024 * 1024

  # Relative subpath for dumping control files inside the temp directory.
  CONTROL_FILE_SUBDIR = os.path.join('autotest', 'au_control_files')

  # The name of the suite of paygen-generated Autotest tests.
  PAYGEN_AU_SUITE_TEMPLATE = 'paygen_au_%s'

  # Name of the Autotest control file tarball.
  CONTROL_TARBALL_TEMPLATE = PAYGEN_AU_SUITE_TEMPLATE + '_control.tar.bz2'

  # Sleep time used in _DiscoverRequiredPayloads. Export so tests can change.
  BUILD_DISCOVER_RETRY_SLEEP = 90

  # Cache of full test payloads for a given version.
  _version_to_full_test_payloads = {}

  class PayloadTest(object):
    """A payload test definition.

    You must either use a delta payload, or specify both the src_channel and
    src_version.

    Attrs:
      payload: A gspaths.Payload object describing the payload to be tested.

      src_channel: The channel of the image to test updating from. Required
                   if the payload is a full payload, required to be None if
                   it's a delta.
      src_version: The version of the image to test updating from. Required
                   if the payload is a full payload, required to be None if
                   it's a delta.
        for a delta payload, as it already encodes the source version.
    """
    def __init__(self, payload, src_channel=None, src_version=None):
      self.payload = payload

      assert bool(src_channel) == bool(src_version), (
          'src_channel(%s), src_version(%s) must both be set, or not set' %
          (src_channel, src_version))

      assert bool(src_channel and src_version) ^ bool(payload.src_image), (
          'src_channel(%s), src_version(%s) required for full, not allowed'
          ' for deltas. src_image: %s ' %
          (src_channel, src_version, payload.src_image))

      self.src_channel = src_channel or payload.src_image.channel
      self.src_version = src_version or payload.src_image.version

    def __str__(self):
      return ('<test for %s%s>' %
              (self.payload,
               (' from version %s' % self.src_version)
               if self.src_version else ''))

    def __repr__(self):
      return str(self)

    def __eq__(self, other):
      return (self.payload == other.payload and
              self.src_channel == other.src_channel and
              self.src_version == other.src_version)

  def __init__(self, build, work_dir, site_config,
               dry_run=False, ignore_finished=False,
               skip_full_payloads=False, skip_delta_payloads=False,
               skip_test_payloads=False, skip_nontest_payloads=False,
               control_dir=None, output_dir=None,
               run_parallel=False, run_on_builder=False, au_generator_uri=None):
    """Initializer."""
    self._build = build
    self._work_dir = work_dir
    self._site_config = site_config
    self._drm = dryrun_lib.DryRunMgr(dry_run)
    self._ignore_finished = dryrun_lib.DryRunMgr(ignore_finished)
    self._skip_full_payloads = skip_full_payloads
    self._skip_delta_payloads = skip_delta_payloads
    self._skip_test_payloads = skip_test_payloads
    self._skip_nontest_payloads = skip_nontest_payloads
    self._control_dir = control_dir
    self._output_dir = output_dir
    self._previous_version = None
    self._run_parallel = run_parallel
    self._run_on_builder = run_on_builder
    self._archive_board = None
    self._archive_build = None
    self._archive_build_uri = None
    self._au_generator_uri = au_generator_uri

  def _GetFlagURI(self, flag):
    """Find the URI of the lock file associated with this build.

    Args:
      flag: Should be a member of gspaths.ChromeosReleases.FLAGS

    Returns:
      Returns a google storage path to the build flag requested.
    """
    return gspaths.ChromeosReleases.BuildPayloadsFlagUri(
        self._build.channel, self._build.board, self._build.version, flag,
        bucket=self._build.bucket)

  def _MapToArchive(self, board, version):
    """Returns the chromeos-image-archive equivalents for the build.

    Args:
      board: The board name (per chromeos-releases).
      version: The build version.

    Returns:
      A tuple consisting of the archive board name, build name and build URI.

    Raises:
      ArchiveError: if we could not compute the mapping.
    """
    # Map chromeos-releases board name to its chromeos-image-archive equivalent.
    archive_board_candidates = set([
        archive_board for archive_board in self._site_config.GetBoards()
        if archive_board.replace('_', '-') == board])
    if len(archive_board_candidates) == 0:
      raise ArchiveError('could not find build board name for %s' % board)
    elif len(archive_board_candidates) > 1:
      raise ArchiveError('found multiple build board names for %s: %s' %
                         (board, ', '.join(archive_board_candidates)))

    archive_board = archive_board_candidates.pop()

    # Find something in the respective chromeos-image-archive build directory.
    archive_build_search_uri = gspaths.ChromeosImageArchive.BuildUri(
        archive_board, '*', version)
    archive_build_file_uri_list = urilib.ListFiles(archive_build_search_uri)
    if not archive_build_file_uri_list:
      raise ArchiveError('cannot find archive build directory for %s' %
                         archive_build_search_uri)

    # Use the first search result.
    uri_parts = urlparse.urlsplit(archive_build_file_uri_list[0])
    archive_build_path = os.path.dirname(uri_parts.path)
    archive_build = archive_build_path.strip('/')
    archive_build_uri = urlparse.urlunsplit((uri_parts.scheme,
                                             uri_parts.netloc,
                                             archive_build_path,
                                             '', ''))

    return archive_board, archive_build, archive_build_uri

  def _ValidateExpectedBuildImages(self, build, images):
    """Validate that we got the expected images for a build.

    We expect that for any given build will have at most the following four
    builds:

      premp basic build.
      mp basic build.
      premp NPO build.
      mp NPO build.

    We also expect that it will have at least one basic build, and never have
    an NPO build for which it doesn't have a matching basic build.

    Args:
      build: The build the images are from.
      images: The images discovered associated with the build.

    Raises:
      BuildCorrupt: Raised if unexpected images are found.
      ImageMissing: Raised if expected images are missing.
    """

    premp_basic = _FilterForBasic(_FilterForPremp(images))
    premp_npo = _FilterForNpo(_FilterForPremp(images))
    mp_basic = _FilterForBasic(_FilterForMp(images))
    mp_npo = _FilterForNpo(_FilterForMp(images))

    # Make sure there is no more than one of each of our basic types.
    for i in (premp_basic, premp_npo, mp_basic, mp_npo):
      if len(i) > 1:
        msg = '%s has unexpected filtered images: %s.' % (build, i)
        raise BuildCorrupt(msg)

    # Make sure there were no unexpected types of images.
    if len(images) != len(premp_basic + premp_npo + mp_basic + mp_npo):
      msg = '%s has unexpected unfiltered images: %s' % (build, images)
      raise BuildCorrupt(msg)

    # Make sure there is at least one basic image.
    if not premp_basic and not mp_basic:
      msg = '%s has no basic images.' % build
      raise ImageMissing(msg)

    # Can't have a premp NPO with the match basic image.
    if premp_npo and not premp_basic:
      msg = '%s has a premp NPO, but not a premp basic image.' % build
      raise ImageMissing(msg)

    # Can't have an mp NPO with the match basic image.
    if mp_npo and not mp_basic:
      msg = '%s has a mp NPO, but not a mp basic image.' % build
      raise ImageMissing(msg)

  def _DiscoverImages(self, build):
    """Return a list of images associated with a given build.

    Args:
      build: The build to find images for.

    Returns:
      A list of images associated with the build. This may include premp, mp,
      and premp/mp NPO images. We don't currently ever expect more than these
      four combinations to be present.

    Raises:
      BuildCorrupt: Raised if unexpected images are found.
      ImageMissing: Raised if expected images are missing.
    """
    # Ideally, |image_type| below should be constrained to the type(s) expected
    # for the board. But the board signing configs are not easily accessible at
    # this point, so we use the wildcard here and rely on the signers to upload
    # the expected artifacts.
    search_uri = gspaths.ChromeosReleases.ImageUri(
        build.channel, build.board, build.version, key='*', image_type='*',
        image_channel='*', image_version='*', bucket=build.bucket)

    image_uris = urilib.ListFiles(search_uri)
    images = [gspaths.ChromeosReleases.ParseImageUri(uri) for uri in image_uris]

    # Unparsable URIs will result in Nones; filter them out.
    images = [i for i in images if i]

    # We only care about recovery and test image types, ignore all others.
    images = _FilterForValidImageType(images)

    self._ValidateExpectedBuildImages(build, images)

    return images

  def _DiscoverTestImageArchives(self, build):
    """Return a list of unsigned image archives associated with a given build.

    Args:
      build: The build to find images for.

    Returns:
      A list of test image archives associated with the build. Normally, there
      should be exactly one such item.

    Raises:
      BuildCorrupt: Raised if unexpected images are found.
      ImageMissing: Raised if expected images are missing.
    """
    search_uri = gspaths.ChromeosReleases.UnsignedImageArchiveUri(
        build.channel, build.board, build.version, milestone='*',
        image_type='test', bucket=build.bucket)

    image_uris = urilib.ListFiles(search_uri)
    images = [gspaths.ChromeosReleases.ParseUnsignedImageArchiveUri(uri)
              for uri in image_uris]

    # Unparsable URIs will result in Nones; filter them out.
    images = [i for i in images if i]

    # Make sure we found the expected number of build images (1).
    if len(images) > 1:
      raise BuildCorrupt('%s has multiple test images: %s' % (build, images))

    if self._control_dir and len(images) < 1:
      raise ImageMissing('%s has no test image' % build)

    return images

  def _DiscoverActiveFsiBuilds(self):
    """Read fsi_images in release.conf.

    fsi_images is a list of chromeos versions. We assume each one is
    from the same build/channel as we are and use it to identify a new
    build. The values in release.conf are only valid for the stable-channel.

    These results only include 'active' FSIs which are still generating a lot
    of update requests. We normally expect to generate delta payloads for
    these FSIs.

    Returns:
      List of gspaths.Build instances for each build so discovered. The list
      may be empty.
    """
    results = []

    # FSI versions are only defined for the stable-channel.
    if self._build.channel != 'stable-channel':
      return results

    contents = json.loads(gslib.Cat(FSI_URI))

    for fsi in contents.get('fsis', []):
      fsi_active = fsi['board']['is_active']
      fsi_board = fsi['board']['public_codename']
      fsi_version = fsi['chrome_os_version']
      fsi_support_delta = fsi['is_delta_supported']

      if fsi_active and fsi_support_delta and fsi_board == self._build.board:
        results.append(gspaths.Build(version=fsi_version,
                                     board=fsi_board,
                                     channel=self._build.channel,
                                     bucket=self._build.bucket))

    return results

  def _DiscoverAllFsiBuilds(self):
    """Pull FSI list from Golden Eye.

    Returns a list of chromeos versions. We assume each one is
    from the same build/channel as we are and use it to identify a new
    build. This assumption is currently valid, but not 100% safe.

    Returns a list of all FSI images for a given board, even 'inactive' values.

    Returns:
      List of gspaths.Build instances for each build so discovered. The list
      may be empty.
    """
    results = []
    contents = json.loads(gslib.Cat(FSI_URI))

    for fsi in contents.get('fsis', []):
      fsi_board = fsi['board']['public_codename']
      fsi_version = fsi['chrome_os_version']
      fsi_lab_stable = fsi['is_lab_stable']

      if fsi_lab_stable and fsi_board == self._build.board:
        results.append(fsi_version)

    return results

  def _DiscoverNmoBuild(self):
    """Find the currently published version to our channel/board.

    We assume it was actually built with our current channel/board. This also
    updates an object member with the previous build, in the case that
    subsequent logic needs to make use of this knowledge.

    Returns:
      List of gspaths.Build for previously published builds. Since we can only
      know about the currently published version, this always contain zero or
      one entries.
    """
    results = []

    # Paygen channel names typically end in '-channel', while Goldeneye
    # does not maintain the '-channel' ending.
    channel_name = self._build.channel.replace('-channel', '')

    contents = json.loads(gslib.Cat(OMAHA_URI))
    for nmo in contents.get('omaha_data', []):
      nmo_board = nmo['board']['public_codename']
      nmo_channel = nmo['channel']
      nmo_version = nmo['chrome_os_version']

      if nmo_board == self._build.board and nmo_channel == channel_name:
        results.append(gspaths.Build(gspaths.Build(version=nmo_version,
                                                   board=self._build.board,
                                                   channel=self._build.channel,
                                                   bucket=self._build.bucket)))

    return results

  def _DiscoverRequiredFullPayloads(self, images):
    """Find the Payload objects for the images from the current build.

    In practice, this creates a full payload definition for every image passed
    in.

    Args:
      images: The images for the current build.

    Returns:
      A list of gspaths.Payload objects for full payloads for every image.
    """
    return [gspaths.Payload(tgt_image=i) for i in images]

  def _DiscoverRequiredNpoDeltas(self, images):
    """Find the NPO deltas for the images from the current build.

    Images from the current build, already filtered to be all MP or all PREMP.

    Args:
      images: The key-filtered images for the current build.

    Returns:
      A list of gspaths.Payload objects for the deltas needed for NPO testing.
      May be empty.
    """
    basics = _FilterForBasic(images)
    # If previously filtered for premp, and filtered for npo, there can only
    # be one of each.
    assert len(basics) <= 1, 'Unexpected images found %s' % basics
    if basics:
      npos = _FilterForImageType(_FilterForNpo(images), basics[0].image_type)
      assert len(npos) <= 1, 'Unexpected NPO images found %s' % npos
      if npos:
        return [gspaths.Payload(tgt_image=npos[0], src_image=basics[0])]

    return []

  # TODO(garnold) The reason we need this separately from
  # _DiscoverRequiredNpoDeltas is that, with test images, we generate
  # a current -> current delta rather than a real current -> NPO one (there are
  # no test NPO images generated, unfortunately). Also, the naming of signed
  # images is different from that of test image archives, so we need different
  # filtering logic. In all likelihood, we will stop generating NPO deltas with
  # signed images once this feature stabilizes; at this point, there will no
  # longer be any use for a signed NPO.
  def _DiscoverRequiredTestNpoDeltas(self, images):
    """Find the NPO deltas test-equivalent for images from the current build.

    Args:
      images: The pre-filtered test images for the current build.

    Returns:
      A (possibly empty) list of gspaths.Payload objects representing NPO
      deltas of test images.
    """
    # If previously filtered for test images, there must be at most one image.
    assert len(images) <= 1, 'Unexpected test images found %s' % images

    if images:
      return [gspaths.Payload(tgt_image=images[0], src_image=images[0])]

    return []

  def _DiscoverRequiredFromPreviousDeltas(self, images, previous_images):
    """Find the deltas from previous builds.

    All arguements should already be filtered to be all MP or all PREMP.

    Args:
      images: The key-filtered images for the current build.
      previous_images: The key-filtered images from previous builds from
                       which delta payloads should be generated.

    Returns:
      A list of gspaths.Payload objects for the deltas needed from the previous
      builds, which may be empty.
    """
    # If we have no images to delta to, no results.
    if not images:
      return []

    # After filtering for NPO, and for MP/PREMP, there can be only one!
    assert len(images) == 1, 'Unexpected images found %s.' % images
    image = images[0]
    # Filter artifacts that have the same |image_type| as that of |image|.
    previous_images_by_type = _FilterForImageType(previous_images,
                                                  image.image_type)

    results = []

    # We should never generate downgrades, they are unsafe. Deltas to the
    # same images are useless. Neither case normally happens unless
    # we are re-generating payloads for old builds.
    for prev in previous_images_by_type:
      if gspaths.VersionGreater(image.version, prev.version):
        # A delta from each previous image to current image.
        results.append(gspaths.Payload(tgt_image=image, src_image=prev))
      else:
        logging.info('Skipping %s is not older than target', prev)

    return results

  def _DiscoverRequiredPayloads(self):
    """Find the payload definitions for the current build.

    This method finds the images for the current build, and for all builds we
    need deltas from, and decides what payloads are needed.

    IMPORTANT: The order in which payloads are listed is significant as it
    reflects on the payload generation order. The current way is to list test
    payloads last, as they are of lesser importance from the release process
    standpoint, and may incur failures that do not affect the signed payloads
    and may be otherwise detrimental to the release schedule.

    Returns:
      A list of tuples of the form (payload, skip), where payload is an
      instance of gspath.Payload and skip is a Boolean that says whether it
      should be skipped (i.e. not generated).

    Raises:
      BuildNotReady: If the current build doesn't seem to have all of it's
          images available yet. This commonly happens because the signer hasn't
          finished signing the current build.
      BuildCorrupt: If current or previous builds have unexpected images.
      ImageMissing: Raised if expected images are missing for previous builds.
    """
    # Initiate a list that will contain lists of payload subsets, along with a
    # Boolean stating whether or not we need to skip generating them.
    payload_sublists_skip = []

    try:
      # When discovering the images for our current build, they might
      # discoverable right away (GS eventual consistency). So, we retry.
      images = retry_util.RetryException(ImageMissing, 3,
                                         self._DiscoverImages, self._build,
                                         sleep=self.BUILD_DISCOVER_RETRY_SLEEP)
      images += self._DiscoverTestImageArchives(self._build)
    except ImageMissing as e:
      # If the main build doesn't have the final build images, then it's
      # not ready.
      logging.info(e)
      raise BuildNotReady()

    _LogList('Images found', images)

    # Discover active FSI builds we need deltas from.
    fsi_builds = self._DiscoverActiveFsiBuilds()
    if fsi_builds:
      _LogList('Active FSI builds considered', fsi_builds)
    else:
      logging.info('No active FSI builds found')

    # Discover other previous builds we need deltas from.
    previous_builds = [b for b in self._DiscoverNmoBuild()
                       if b not in fsi_builds]
    if previous_builds:
      _LogList('Other previous builds considered', previous_builds)
    else:
      logging.info('No other previous builds found')

    # Discover the images from those previous builds, and put them into
    # a single list. Raises ImageMissing if no images are found.
    previous_images = []
    for b in previous_builds:
      try:
        previous_images += self._DiscoverImages(b)
      except ImageMissing as e:
        # Temporarily allow generation of delta payloads to fail because of
        # a missing previous build until crbug.com/243916 is addressed.
        # TODO(mtennant): Remove this when bug is fixed properly.
        logging.warning('Previous build image is missing, skipping: %s', e)

        # We also clear the previous version field so that subsequent code does
        # not attempt to generate a full update test from the N-1 version;
        # since this version has missing images, no payloads were generated for
        # it and test generation is bound to fail.
        # TODO(garnold) This should be reversed together with the rest of this
        # block.
        self._previous_version = None

        # In this case, we should also skip test image discovery; since no
        # signed deltas will be generated from this build, we don't need to
        # generate test deltas from it.
        continue

      previous_images += self._DiscoverTestImageArchives(b)

    for b in fsi_builds:
      previous_images += self._DiscoverImages(b)
      previous_images += self._DiscoverTestImageArchives(b)

    # Only consider base (signed) and test previous images.
    filtered_previous_images = _FilterForBasic(previous_images)
    filtered_previous_images += _FilterForTest(previous_images)
    previous_images = filtered_previous_images

    # Generate full payloads for all non-test images in the current build.
    # Include base, NPO, premp, and mp (if present).
    payload_sublists_skip.append(
        (self._skip_full_payloads or self._skip_nontest_payloads,
         self._DiscoverRequiredFullPayloads(_FilterForImages(images))))

    # Deltas for current -> NPO (pre-MP and MP).
    payload_sublists_skip.append(
        (self._skip_delta_payloads or self._skip_nontest_payloads,
         self._DiscoverRequiredNpoDeltas(_FilterForPremp(images))))
    payload_sublists_skip.append(
        (self._skip_delta_payloads or self._skip_nontest_payloads,
         self._DiscoverRequiredNpoDeltas(_FilterForMp(images))))

    # Deltas for previous -> current (pre-MP and MP).
    payload_sublists_skip.append(
        (self._skip_delta_payloads or self._skip_nontest_payloads,
         self._DiscoverRequiredFromPreviousDeltas(
             _FilterForPremp(_FilterForBasic(images)),
             _FilterForPremp(previous_images))))
    payload_sublists_skip.append(
        (self._skip_delta_payloads or self._skip_nontest_payloads,
         self._DiscoverRequiredFromPreviousDeltas(
             _FilterForMp(_FilterForBasic(images)),
             _FilterForMp(previous_images))))

    # Only discover test payloads if Autotest is not disabled.
    if self._control_dir:
      # Full test payloads.
      payload_sublists_skip.append(
          (self._skip_full_payloads or self._skip_test_payloads,
           self._DiscoverRequiredFullPayloads(_FilterForTest(images))))

      # Delta for current -> NPO (test payloads).
      payload_sublists_skip.append(
          (self._skip_delta_payloads or self._skip_test_payloads,
           self._DiscoverRequiredTestNpoDeltas(_FilterForTest(images))))

      # Deltas for previous -> current (test payloads).
      payload_sublists_skip.append(
          (self._skip_delta_payloads or self._skip_test_payloads,
           self._DiscoverRequiredFromPreviousDeltas(
               _FilterForTest(images), _FilterForTest(previous_images))))

    # Organize everything into a single list of (payload, skip) pairs; also, be
    # sure to fill in a URL for each payload.
    payloads_skip = []
    for (do_skip, payloads) in payload_sublists_skip:
      for payload in payloads:
        paygen_payload_lib.FillInPayloadUri(payload)
        payloads_skip.append((payload, do_skip))

    return payloads_skip

  def _GeneratePayloads(self, payloads, lock=None):
    """Generate the payloads called for by a list of payload definitions.

    It will keep going, even if there is a failure.

    Args:
      payloads: gspath.Payload objects defining all of the payloads to generate.
      lock: gslock protecting this paygen_build run.

    Raises:
      Any arbitrary exception raised by CreateAndUploadPayload.
    """
    payloads_args = [(payload,
                      self._work_dir,
                      isinstance(payload.tgt_image, gspaths.Image),
                      self._au_generator_uri,
                      bool(self._drm))
                     for payload in payloads]

    if self._run_parallel:
      parallel.RunTasksInProcessPool(_GenerateSinglePayload, payloads_args)
    else:
      for args in payloads_args:
        _GenerateSinglePayload(*args)

        # This can raise LockNotAcquired, if the lock timed out during a
        # single payload generation.
        if lock:
          lock.Renew()

  def _FindFullTestPayloads(self, channel, version):
    """Returns a list of full test payloads for a given version.

    Uses the current build's board and bucket values. This method caches the
    full test payloads previously discovered as we may be using them for
    multiple tests in a single run.

    Args:
      channel: Channel to look in for payload.
      version: A build version whose payloads to look for.

    Returns:
      A (possibly empty) list of payload URIs.
    """
    assert channel
    assert version

    if (channel, version) in self._version_to_full_test_payloads:
      # Serve from cache, if possible.
      return self._version_to_full_test_payloads[(channel, version)]

    payload_search_uri = gspaths.ChromeosReleases.PayloadUri(
        channel, self._build.board, version, '*',
        bucket=self._build.bucket)

    payload_candidate = urilib.ListFiles(payload_search_uri)

    # We create related files for each payload that have the payload name
    # plus these extensions. Skip these files.
    NOT_PAYLOAD = ('.json', '.log')
    full_test_payloads = [u for u in payload_candidate
                          if not any([u.endswith(n) for n in NOT_PAYLOAD])]
    # Store in cache.
    self._version_to_full_test_payloads[(channel, version)] = full_test_payloads
    return full_test_payloads

  def _EmitControlFile(self, payload_test, suite_name, control_dump_dir):
    """Emit an Autotest control file for a given payload test."""
    # Figure out the source version for the test.
    payload = payload_test.payload
    src_version = payload_test.src_version
    src_channel = payload_test.src_channel

    # Discover the full test payload that corresponds to the source version.
    src_payload_uri_list = self._FindFullTestPayloads(src_channel, src_version)
    if not src_payload_uri_list:
      logging.error('Cannot find full test payload for source version (%s), '
                    'control file not generated', src_version)
      raise PayloadTestError('cannot find source payload for testing %s' %
                             payload)

    if len(src_payload_uri_list) != 1:
      logging.error('Found multiple (%d) full test payloads for source version '
                    '(%s), control file not generated:\n%s',
                    len(src_payload_uri_list), src_version,
                    '\n'.join(src_payload_uri_list))
      raise PayloadTestError('multiple source payloads found for testing %s' %
                             payload)

    src_payload_uri = src_payload_uri_list[0]
    logging.info('Source full test payload found at %s', src_payload_uri)

    release_archive_uri = gspaths.ChromeosReleases.BuildUri(
        src_channel, self._build.board, src_version)

    # TODO(dgarrett): Remove if block after finishing crbug.com/523122
    if not urilib.Exists(os.path.join(release_archive_uri, 'stateful.tgz')):
      logging.warning('Falling back to chromeos-image-archive: %s', payload)
      try:
        _, _, source_archive_uri = self._MapToArchive(
            payload.tgt_image.board, src_version)
      except ArchiveError as e:
        raise PayloadTestError(
            'error mapping source build to images archive: %s' % e)
      stateful_archive_uri = os.path.join(source_archive_uri, 'stateful.tgz')
      logging.info('Copying stateful.tgz from %s -> %s',
                   stateful_archive_uri, release_archive_uri)
      urilib.Copy(stateful_archive_uri, release_archive_uri)

    test = test_params.TestConfig(
        self._archive_board,
        suite_name,               # Name of the test (use the suite name).
        False,                    # Using test images.
        bool(payload.src_image),  # Whether this is a delta.
        src_version,
        payload.tgt_image.version,
        src_payload_uri,
        payload.uri,
        suite_name=suite_name,
        source_archive_uri=release_archive_uri)

    with open(test_control.get_control_file_name()) as f:
      control_code = f.read()
    control_file = test_control.dump_autotest_control_file(
        test, None, control_code, control_dump_dir)
    logging.info('Control file emitted at %s', control_file)
    return control_file

  def _ScheduleAutotestTests(self, suite_name):
    """Run the appropriate command to schedule the Autotests we have prepped.

    Args:
      suite_name: The name of the test suite.
    """
    # Because of crbug.com/383481, if we run delta updates against
    # a source payload earlier than R38, the DUTs assigned to the
    # test may fail when rebooting.  The failed devices can be hard
    # to recover.  The bug affects spring and skate, but as of this
    # writing, only spring is still losing devices.
    #
    # So, until it's all sorted, we're going to skip testing on
    # spring devices.
    if self._archive_board == 'daisy_spring':
      logging.info('Skipping payload autotest for board %s',
                   self._archive_board)
      return
    timeout_mins = config_lib.HWTestConfig.DEFAULT_HW_TEST_TIMEOUT / 60
    if self._run_on_builder:
      try:
        commands.RunHWTestSuite(board=self._archive_board,
                                build=self._archive_build,
                                suite=suite_name,
                                file_bugs=True,
                                pool='bvt',
                                priority=constants.HWTEST_BUILD_PRIORITY,
                                retry=True,
                                wait_for_results=True,
                                timeout_mins=timeout_mins,
                                suite_min_duts=2,
                                debug=bool(self._drm))
      except failures_lib.TestWarning as e:
        logging.warning('Warning running test suite; error output:\n%s', e)
    else:
      # Run run_suite.py locally.
      cmd = [
          os.path.join(AUTOTEST_DIR, 'site_utils', 'run_suite.py'),
          '--board', self._archive_board,
          '--build', self._archive_build,
          '--suite_name', suite_name,
          '--file_bugs', 'True',
          '--pool', 'bvt',
          '--retry', 'True',
          '--timeout_mins', str(timeout_mins),
          '--no_wait', 'False',
          '--suite_min_duts', '2',
      ]
      logging.info('Running autotest suite: %s', ' '.join(cmd))
      try:
        cros_build_lib.RunCommand(cmd)
      except cros_build_lib.RunCommandError as e:
        if e.result.returncode:
          logging.error('Error (%d) running test suite; error output:\n%s',
                        e.result.returncode, e.result.error)
          raise PayloadTestError('failed to run test (return code %d)' %
                                 e.result.returncode)

  def _AutotestPayloads(self, payload_tests):
    """Create necessary test artifacts and initiate Autotest runs.

    Args:
      payload_tests: An iterable of PayloadTest objects defining payload tests.
    """
    # Create inner hierarchy for dumping Autotest control files.
    control_dump_dir = os.path.join(self._control_dir,
                                    self.CONTROL_FILE_SUBDIR)
    os.makedirs(control_dump_dir)

    # Customize the test suite's name based on this build's channel.
    test_channel = self._build.channel.rpartition('-')[0]
    suite_name = (self.PAYGEN_AU_SUITE_TEMPLATE % test_channel)

    # Emit a control file for each payload.
    logging.info('Emitting control files into %s', control_dump_dir)
    for payload_test in payload_tests:
      self._EmitControlFile(payload_test, suite_name, control_dump_dir)

    tarball_name = self.CONTROL_TARBALL_TEMPLATE % test_channel

    # Must use an absolute tarball path since tar is run in a different cwd.
    tarball_path = os.path.join(self._control_dir, tarball_name)

    # Create the tarball.
    logging.info('Packing %s in %s into %s', self.CONTROL_FILE_SUBDIR,
                 self._control_dir, tarball_path)
    cmd_result = cros_build_lib.CreateTarball(
        tarball_path, self._control_dir,
        compression=cros_build_lib.COMP_BZIP2,
        inputs=[self.CONTROL_FILE_SUBDIR])
    if cmd_result.returncode != 0:
      logging.error('Error (%d) when tarring control files',
                    cmd_result.returncode)
      raise PayloadTestError(
          'failed to create autotest tarball (return code %d)' %
          cmd_result.returncode)

    # Upload the tarball, be sure to make it world-readable.
    upload_target = os.path.join(self._archive_build_uri, tarball_name)
    logging.info('Uploading autotest control tarball to %s', upload_target)
    gslib.Copy(tarball_path, upload_target, acl='public-read')

    # Do not run the suite for older builds whose suite staging logic is
    # broken.  We use the build's milestone number as a rough estimate to
    # whether or not it's recent enough. We derive the milestone number from
    # the archive build name, which takes the form
    # boardname-release/R12-3456.78.9 (in this case it is 12).
    try:
      build_mstone = int(self._archive_build.partition('/')[2]
                         .partition('-')[0][1:])
      if build_mstone < RUN_SUITE_MIN_MSTONE:
        logging.warning('Build milestone < %s, test suite scheduling skipped',
                        RUN_SUITE_MIN_MSTONE)
        return
    except ValueError:
      raise PayloadTestError(
          'Failed to infer archive build milestone number (%s)' %
          self._archive_build)

    # Actually have the tests run.
    self._ScheduleAutotestTests(suite_name)

  @staticmethod
  def _IsTestDeltaPayload(payload):
    """Returns True iff a given payload is a test delta one."""
    return (payload.tgt_image.get('image_type', 'signed') != 'signed' and
            payload.src_image is not None)

  def _CreateFsiPayloadTests(self, payload, fsi_versions):
    """Create PayloadTests against a list of board FSIs.

    Args:
      payload: The payload we are trying to test.
      fsi_versions: The list of known FSIs for this board.

    Returns:
      A list of PayloadTest objects to test with, may be empty.
    """
    # Make sure we try oldest FSIs first for testing.
    fsi_versions = sorted(fsi_versions, key=gspaths.VersionKey)
    logging.info('Considering FSI tests against: %s', ', '.join(fsi_versions))

    for fsi in fsi_versions:
      # If the FSI is newer than what we are generating, skip it.
      if gspaths.VersionGreater(fsi, payload.tgt_image.version):
        logging.info(
            '  FSI newer than payload, Skipping FSI test against: %s', fsi)
        continue

      # Validate that test artifacts exist. The results are thrown away.
      if not self._FindFullTestPayloads('stable-channel', fsi):
        # Some of our old FSIs have no test artifacts, so not finding them
        # isn't an error. Skip that FSI and try the next.
        logging.info('  No artifacts, skipping FSI test against: %s', fsi)
        continue

      logging.info('  Scheduling FSI test against: %s', fsi)
      return [self.PayloadTest(
          payload, src_channel='stable-channel', src_version=fsi)]

    # If there are no FSIs, or no testable FSIs, no tests.
    logging.info('No FSIs with artifacts, not scheduling FSI update test.')
    return []

  def _CreatePayloadTests(self, payloads):
    """Returns a list of test configurations for a given list of payloads.

    Args:
      payloads: A list of (already generated) build payloads.

    Returns:
      A list of PayloadTest objects defining payload test cases.
    """
    payload_tests = []
    for payload in payloads:
      # We are only testing test payloads.
      if payload.tgt_image.get('image_type', 'signed') == 'signed':
        continue

      # Distinguish between delta (source version encoded) and full payloads.
      if payload.src_image is None:
        # Create a full update test from NMO, if we are newer.
        if not self._previous_version:
          logging.warning('No previous build, not testing full update %s from '
                          'NMO', payload)
        elif gspaths.VersionGreater(
            self._previous_version, payload.tgt_image.version):
          logging.warning(
              'NMO (%s) is newer than target (%s), skipping NMO full '
              'update test.', self._previous_version, payload)
        else:
          payload_tests.append(self.PayloadTest(
              payload, src_channel=self._build.channel,
              src_version=self._previous_version))

        # Create a full update test from the current version to itself.
        payload_tests.append(self.PayloadTest(
            payload,
            src_channel=self._build.channel,
            src_version=self._build.version))

        # Create a full update test from oldest viable FSI.
        payload_tests += self._CreateFsiPayloadTests(
            payload, self._DiscoverAllFsiBuilds())
      else:
        # Create a delta update test.
        payload_tests.append(self.PayloadTest(payload))

    return payload_tests

  def _CleanupBuild(self):
    """Clean up any leaked temp files associated with this build in GS."""
    # Clean up any signer client files that leaked on this or previous
    # runs.
    self._drm(gslib.Remove,
              gspaths.ChromeosReleases.BuildPayloadsSigningUri(
                  self._build.channel, self._build.board, self._build.version,
                  bucket=self._build.bucket),
              recurse=True, ignore_no_match=True)

  def CreatePayloads(self):
    """Get lock on this build, and Process if we succeed.

    While holding the lock, check assorted build flags to see if we should
    process this build.

    Raises:
      BuildSkip: If the build was marked with a skip flag.
      BuildFinished: If the build was already marked as finished.
      BuildLocked: If the build is locked by another server or process.
    """
    lock_uri = self._GetFlagURI(gspaths.ChromeosReleases.LOCK)
    skip_uri = self._GetFlagURI(gspaths.ChromeosReleases.SKIP)
    finished_uri = self._GetFlagURI(gspaths.ChromeosReleases.FINISHED)

    logging.info('Examining: %s', self._build)

    try:
      with gslock.Lock(lock_uri, dry_run=bool(self._drm)) as build_lock:
        # If the build was marked to skip, skip
        if gslib.Exists(skip_uri):
          raise BuildSkip()

        # If the build was already marked as finished, we're finished.
        if self._ignore_finished(gslib.Exists, finished_uri):
          raise BuildFinished()

        logging.info('Starting: %s', self._build)

        payloads_skip = self._DiscoverRequiredPayloads()

        # Assume we can finish the build until we find a reason we can't.
        can_finish = True

        if self._output_dir:
          can_finish = False

        # Find out which payloads already exist, updating the payload object's
        # URI accordingly. In doing so we're creating a list of all payload
        # objects and their skip/exist attributes. We're also recording whether
        # this run will be skipping any actual work.
        payloads_attrs = []
        for payload, skip in payloads_skip:
          if self._output_dir:
            # output_dir means we are forcing all payloads to be generated
            # with a new destination.
            result = [os.path.join(self._output_dir,
                                   os.path.basename(payload.uri))]
            exists = False
          else:
            result = paygen_payload_lib.FindExistingPayloads(payload)
            exists = bool(result)

          if result:
            paygen_payload_lib.SetPayloadUri(payload, result[0])
          elif skip:
            can_finish = False

          payloads_attrs.append((payload, skip, exists))

        # Display payload generation list, including payload name and whether
        # or not it already exists or will be skipped.
        log_items = []
        for payload, skip, exists in payloads_attrs:
          desc = str(payload)
          if exists:
            desc += ' (exists)'
          elif skip:
            desc += ' (skipped)'
          log_items.append(desc)

        _LogList('All payloads for the build', log_items)

        # Generate new payloads.
        new_payloads = [payload for payload, skip, exists in payloads_attrs
                        if not (skip or exists)]
        if new_payloads:
          logging.info('Generating %d new payload(s)', len(new_payloads))
          self._GeneratePayloads(new_payloads, build_lock)
        else:
          logging.info('No new payloads to generate')

        # Test payloads.
        if not self._control_dir:
          logging.info('Payload autotesting disabled.')
        elif not can_finish:
          logging.warning('Not all payloads were generated/uploaded, '
                          'skipping payload autotesting.')
        else:
          try:
            # Check that the build has a corresponding archive directory. If it
            # does not, then testing should not be attempted.
            archive_board, archive_build, archive_build_uri = (
                self._MapToArchive(self._build.board, self._build.version))
            self._archive_board = archive_board
            self._archive_build = archive_build
            self._archive_build_uri = archive_build_uri

            # We have a control file directory and all payloads have been
            # generated. Lets create the list of tests to conduct.
            payload_tests = self._CreatePayloadTests(
                [payload for payload, _, _ in payloads_attrs])
            if payload_tests:
              logging.info('Initiating %d payload tests', len(payload_tests))
              self._drm(self._AutotestPayloads, payload_tests)
          except ArchiveError as e:
            logging.warning('Cannot map build to images archive, skipping '
                            'payload autotesting.')
            can_finish = False

        self._CleanupBuild()
        if can_finish:
          self._drm(gslib.CreateWithContents, finished_uri,
                    socket.gethostname())
        else:
          logging.warning('Not all payloads were generated, uploaded or '
                          'tested; not marking build as finished')

        logging.info('Finished: %s', self._build)

    except gslock.LockNotAcquired as e:
      logging.info('Build already being processed: %s', e)
      raise BuildLocked()

    except EarlyExit:
      logging.info('Nothing done: %s', self._build)
      raise

    except Exception:
      logging.error('Failed: %s', self._build)
      raise


def _FindControlFileDir(work_dir):
  """Decide the directory for emitting control files.

  If a working directory is passed in, we create a unique directory inside
  it; otherwise we use Python's default tempdir.

  Args:
    work_dir: Create the control file directory here (None for the default).

  Returns:
    Path to a unique directory that the caller is responsible for cleaning up.
  """
  # Setup assorted working directories.
  # It is safe for multiple parallel instances of paygen_payload to share the
  # same working directory.
  if work_dir and not os.path.exists(work_dir):
    os.makedirs(work_dir)

  # If work_dir is None, then mkdtemp will use '/tmp'
  return tempfile.mkdtemp(prefix='paygen_build-control_files.', dir=work_dir)


def ValidateBoardConfig(board):
  """Validate that we have config values for the specified |board|.

  Args:
    board: Name of board to check.

  Raises:
    BoardNotConfigured if the board is unknown.
  """
  # Right now, we just validate that the board exists.
  boards = json.loads(gslib.Cat(BOARDS_URI))
  for b in boards.get('boards', []):
    if b['public_codename'] == board:
      return
  raise BoardNotConfigured(board)


def CreatePayloads(build, work_dir, site_config, dry_run=False,
                   ignore_finished=False, skip_full_payloads=False,
                   skip_delta_payloads=False, skip_test_payloads=False,
                   skip_nontest_payloads=False, disable_tests=False,
                   output_dir=None, run_parallel=False, run_on_builder=False,
                   au_generator_uri=None):
  """Helper method than generates payloads for a given build.

  Args:
    build: gspaths.Build instance describing the build to generate payloads for.
    work_dir: Directory to contain both scratch and long-term work files.
    dry_run: Do not generate payloads (optional).
    site_config: A valid SiteConfig. Only used to map board names.
    ignore_finished: Ignore the FINISHED flag (optional).
    skip_full_payloads: Do not generate full payloads.
    skip_delta_payloads: Do not generate delta payloads.
    skip_test_payloads: Do not generate test payloads.
    skip_nontest_payloads: Do not generate non-test payloads.
    disable_tests: Do not attempt generating test artifacts or running tests.
    output_dir: Directory for payload files, or None for GS default locations.
    run_parallel: Generate payloads in parallel processes.
    run_on_builder: Running in a cbuildbot environment on a builder.
    au_generator_uri: URI of au_generator.zip to use, None to use the default.
  """
  ValidateBoardConfig(build.board)

  control_dir = None
  try:
    if not disable_tests:
      control_dir = _FindControlFileDir(work_dir)

    _PaygenBuild(build, work_dir, site_config,
                 dry_run=dry_run,
                 ignore_finished=ignore_finished,
                 skip_full_payloads=skip_full_payloads,
                 skip_delta_payloads=skip_delta_payloads,
                 skip_test_payloads=skip_test_payloads,
                 skip_nontest_payloads=skip_nontest_payloads,
                 control_dir=control_dir, output_dir=output_dir,
                 run_parallel=run_parallel,
                 run_on_builder=run_on_builder,
                 au_generator_uri=au_generator_uri).CreatePayloads()

  finally:
    if control_dir:
      shutil.rmtree(control_dir)
