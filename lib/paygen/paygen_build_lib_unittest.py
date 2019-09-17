# -*- coding: utf-8 -*-
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for paygen_build_lib."""

from __future__ import print_function

import json
import os
import tarfile
import sys

import mock

from chromite.cbuildbot import commands
from chromite.lib import config_lib_unittest
from chromite.lib import constants
from chromite.lib import cros_test_lib
from chromite.lib import gs
from chromite.lib import parallel

from chromite.lib.paygen import gslock
from chromite.lib.paygen import gspaths
from chromite.lib.paygen import paygen_build_lib
from chromite.lib.paygen import paygen_payload_lib

AUTOTEST_DIR = os.path.join(constants.SOURCE_ROOT, 'src', 'third_party',
                            'autotest', 'files')
sys.path.insert(0, AUTOTEST_DIR)
# pylint: disable=import-error,wrong-import-position
from site_utils.autoupdate.lib import test_params

# We access a lot of protected members during testing.
# pylint: disable=protected-access


class BasePaygenBuildLibTest(cros_test_lib.MockTestCase):
  """Base class for testing PaygenBuildLib class."""

  def setUp(self):
    # Clear json cache.
    paygen_build_lib.PaygenBuild._cachedPaygenJson = None

    # Mock out fetching of paygen.json from GS.
    self.mockGetJson = self.PatchObject(
        paygen_build_lib, '_GetJson',
        side_effect=lambda _: self.getParseTestPaygenJson())

    # Mock a few more to ensure there is no accidental GS interaction.
    self.mockUriList = self.PatchObject(gs.GSContext, 'LS')

  def getParseTestPaygenJson(self):
    """Fetch raw parsed json from our test copy of paygen.json."""
    # TODO: Add caching, so we don't keep loading/parsing the same file.
    paygen_json_file = os.path.join(os.path.dirname(__file__),
                                    'testdata', 'paygen.json')
    with open(paygen_json_file, 'r') as fp:
      return json.load(fp)


class PaygenJsonTests(BasePaygenBuildLibTest):
  """Test cases that require mocking paygen.json fetching."""

  def testGetPaygenJsonCaching(self):
    expected_paygen_size = 1056
    result = paygen_build_lib.PaygenBuild.GetPaygenJson()
    self.assertEqual(len(result), expected_paygen_size)
    self.mockGetJson.assert_called_once()

    # Validate caching, by proving we don't refetch.
    self.mockGetJson.reset_mock()
    result = paygen_build_lib.PaygenBuild.GetPaygenJson()
    self.assertEqual(len(result), expected_paygen_size)
    self.mockGetJson.assert_not_called()

  def testGetPaygenJsonBoard(self):
    result = paygen_build_lib.PaygenBuild.GetPaygenJson('unknown')
    self.assertEqual(len(result), 0)

    result = paygen_build_lib.PaygenBuild.GetPaygenJson('auron-yuna')
    self.assertEqual(len(result), 13)

  def testGetPaygenJsonBoardChannel(self):
    result = paygen_build_lib.PaygenBuild.GetPaygenJson(
        'auron-yuna', 'unknown')
    self.assertEqual(len(result), 0)

    result = paygen_build_lib.PaygenBuild.GetPaygenJson(
        'auron-yuna', 'canary')
    self.assertEqual(len(result), 1)

    result = paygen_build_lib.PaygenBuild.GetPaygenJson(
        'auron-yuna', 'dev-channel')
    self.assertEqual(len(result), 1)

    result = paygen_build_lib.PaygenBuild.GetPaygenJson(
        'auron-yuna', 'beta-channel')
    self.assertEqual(len(result), 1)

    result = paygen_build_lib.PaygenBuild.GetPaygenJson(
        'auron-yuna', 'stable-channel')
    self.assertEqual(len(result), 10)

    # Prove that we get the same results in both channel namespaces.
    self.assertEqual(
        paygen_build_lib.PaygenBuild.GetPaygenJson(
            'auron-yuna', 'stable'),
        paygen_build_lib.PaygenBuild.GetPaygenJson(
            'auron-yuna', 'stable-channel'))

    # Look up data for a board with no payloads.
    result = paygen_build_lib.PaygenBuild.GetPaygenJson(
        'arkham', 'canary')
    self.assertEqual(len(result), 0)

  def testValidateBoardConfig(self):
    """Test ValidateBoardConfig."""
    # Test a known board works.
    paygen_build_lib.ValidateBoardConfig('chell')

    # Test a known variant board works.
    paygen_build_lib.ValidateBoardConfig('auron-yuna')

    # Test an unknown board doesn't.
    with self.assertRaises(paygen_build_lib.BoardNotConfigured):
      paygen_build_lib.ValidateBoardConfig('unknown')


class BasePaygenBuildLibTestWithBuilds(BasePaygenBuildLibTest,
                                       cros_test_lib.TempDirTestCase):
  """Test PaygenBuildLib class."""

  def setUp(self):
    self.dlc_id = 'dummy-dlc'
    self.dlc_id2 = 'dummy-dlc2'
    self.dlc_package = 'dummy-package'
    self.dlc_package2 = 'dummy-package2'

    self.prev_build = gspaths.Build(bucket='crt',
                                    channel='foo-channel',
                                    board='foo-board',
                                    version='1.0.0')

    self.prev_image = gspaths.Image(build=self.prev_build, key='mp')
    self.prev_premp_image = gspaths.Image(build=self.prev_build, key='premp')
    self.prev_test_image = gspaths.UnsignedImageArchive(
        build=self.prev_build, image_type='test')
    self.prev_dlc_package_image = gspaths.DLCImage(
        build=self.prev_build, key=None, dlc_id=self.dlc_id,
        dlc_package=self.dlc_package,
        dlc_image=gspaths.ChromeosReleases.DLCImageName())
    self.prev_dlc_package2_image = gspaths.DLCImage(
        build=self.prev_build, key=None, dlc_id=self.dlc_id,
        dlc_package=self.dlc_package2,
        dlc_image=gspaths.ChromeosReleases.DLCImageName())
    self.prev_dlc2_image = gspaths.DLCImage(
        build=self.prev_build, key=None, dlc_id=self.dlc_id2,
        dlc_package=self.dlc_package,
        dlc_image=gspaths.ChromeosReleases.DLCImageName())

    self.target_build = gspaths.Build(bucket='crt',
                                      channel='foo-channel',
                                      board='foo-board',
                                      version='1.2.3')

    # Create an additional 'special' image like NPO that isn't NPO,
    # and keyed with a weird key. It should match none of the filters.
    self.special_image = gspaths.Image(build=self.target_build,
                                       key='foo-key',
                                       image_channel='special-channel')

    self.basic_image = gspaths.Image(build=self.target_build, key='mp-v2')
    self.premp_image = gspaths.Image(build=self.target_build, key='premp')
    self.test_image = gspaths.UnsignedImageArchive(
        build=self.target_build, image_type='test')
    self.dlc_image = gspaths.DLCImage(
        build=self.target_build, key=None, dlc_id=self.dlc_id,
        dlc_package=self.dlc_package,
        dlc_image=gspaths.ChromeosReleases.DLCImageName())

    self.mp_full_payload = gspaths.Payload(tgt_image=self.basic_image)
    self.test_full_payload = gspaths.Payload(tgt_image=self.test_image)
    self.mp_delta_payload = gspaths.Payload(tgt_image=self.basic_image,
                                            src_image=self.prev_image)
    self.test_delta_payload = gspaths.Payload(tgt_image=self.test_image,
                                              src_image=self.prev_test_image)

    self.full_payload_test = paygen_build_lib.PayloadTest(
        self.test_full_payload,
        self.target_build.channel,
        self.target_build.version)
    self.delta_payload_test = paygen_build_lib.PayloadTest(
        self.test_delta_payload,
        payload_type=paygen_build_lib.PAYLOAD_TYPE_OMAHA)

  def _GetPaygenBuildInstance(self, dry_run=False, skip_delta_payloads=False):
    """Helper method to create a standard Paygen instance."""
    return paygen_build_lib.PaygenBuild(self.target_build, self.target_build,
                                        self.tempdir,
                                        config_lib_unittest.MockSiteConfig(),
                                        dry_run=dry_run,
                                        skip_delta_payloads=skip_delta_payloads)

  def testGetFlagURI(self):
    """Validate the helper method to create flag URIs for our current build."""
    paygen = self._GetPaygenBuildInstance()

    self.assertEqual(
        paygen._GetFlagURI(gspaths.ChromeosReleases.LOCK),
        'gs://crt/foo-channel/foo-board/1.2.3/payloads/LOCK_flag')

  def testFilterHelpers(self):
    """Test _FilterForMp helper method."""

    # All of the filter helpers should handle empty list.
    self.assertEqual(paygen_build_lib._FilterForMp([]), [])
    self.assertEqual(paygen_build_lib._FilterForPremp([]), [])
    self.assertEqual(paygen_build_lib._FilterForBasic([]), [])

    # prev_image lets us test with an 'mp' key, instead of an 'mp-v2' key.
    images = [self.basic_image, self.premp_image,
              self.special_image, self.prev_image]

    self.assertEqual(paygen_build_lib._FilterForMp(images),
                     [self.basic_image, self.prev_image])

    self.assertEqual(paygen_build_lib._FilterForPremp(images),
                     [self.premp_image])

    self.assertEqual(paygen_build_lib._FilterForBasic(images),
                     [self.basic_image, self.premp_image, self.prev_image])

  def testMapToArchive(self):
    """Test the _MapToArchive method."""
    # TODO

  def testValidateExpectedBuildImages(self):
    """Test a function that validates expected images are found on a build."""
    paygen = self._GetPaygenBuildInstance()

    # Test with basic mp image only.
    paygen._ValidateExpectedBuildImages(self.target_build, (self.basic_image,))

    # Test with basic mp and mp npo images.
    paygen._ValidateExpectedBuildImages(self.target_build, (self.basic_image,))
    # Test with basic mp and premp images.
    paygen._ValidateExpectedBuildImages(self.target_build, (self.basic_image,
                                                            self.premp_image))

    # Test with basic mp and premp images.
    paygen._ValidateExpectedBuildImages(self.target_build, (self.basic_image,
                                                            self.premp_image))

    # Test with 4 different images.
    paygen._ValidateExpectedBuildImages(self.target_build, (self.basic_image,
                                                            self.premp_image))

    # No images isn't valid.
    with self.assertRaises(paygen_build_lib.ImageMissing):
      paygen._ValidateExpectedBuildImages(self.target_build, [])

    # More than one of the same type of image should trigger BuildCorrupt
    with self.assertRaises(paygen_build_lib.BuildCorrupt):
      paygen._ValidateExpectedBuildImages(self.target_build, (self.basic_image,
                                                              self.basic_image))

    # Unexpected images should trigger BuildCorrupt
    with self.assertRaises(paygen_build_lib.BuildCorrupt):
      paygen._ValidateExpectedBuildImages(self.target_build,
                                          (self.basic_image,
                                           self.special_image))

  def testValidateExpectedDLCBuildImages(self):
    """Test a function that validates expected DLC images are found."""

    paygen = self._GetPaygenBuildInstance()
    dlc_image = gspaths.DLCImage(
        build=self.target_build, dlc_id=self.dlc_id,
        dlc_package=self.dlc_package,
        dlc_image=gspaths.ChromeosReleases.DLCImageName())
    paygen._ValidateExpectedDLCBuildImages(self.target_build, (dlc_image,))

  def testDefaultPayloadUri(self):
    """Test paygen_payload_lib.DefaultPayloadUri."""

    # Test a Full Payload
    result = paygen_build_lib._DefaultPayloadUri(self.mp_full_payload,
                                                 random_str='abc123')
    self.assertEqual(
        result,
        'gs://crt/foo-channel/foo-board/1.2.3/payloads/'
        'chromeos_1.2.3_foo-board_foo-channel_full_mp-v2.bin-abc123.signed')

    # Test a Delta Payload
    result = paygen_build_lib._DefaultPayloadUri(self.mp_delta_payload,
                                                 random_str='abc123')
    self.assertEqual(
        result,
        'gs://crt/foo-channel/foo-board/1.2.3/payloads/chromeos_1.0.0-1'
        '.2.3_foo-board_foo-channel_delta_mp-v2.bin-abc123.signed')

    # Test changing channel, board, and keys
    src_image = gspaths.Image(build=gspaths.Build(channel='dev-channel',
                                                  board='x86-alex',
                                                  version='3588.0.0',
                                                  bucket='crt'),
                              key='premp')
    tgt_image = gspaths.Image(build=gspaths.Build(channel='stable-channel',
                                                  board='x86-alex-he',
                                                  version='3590.0.0',
                                                  bucket='crt'),
                              key='mp-v3')
    payload = gspaths.Payload(src_image=src_image, tgt_image=tgt_image)

    result = paygen_build_lib._DefaultPayloadUri(payload, random_str='abc123')
    self.assertEqual(
        result,
        'gs://crt/stable-channel/x86-alex-he/3590.0.0/payloads/'
        'chromeos_3588.0.0-3590.0.0_x86-alex-he_stable-channel_delta_mp-v3.bin-'
        'abc123.signed')


class TestPaygenBuildLibTestGSSearch(BasePaygenBuildLibTestWithBuilds):
  """Test discovery of images."""

  def testFillInPayloadUri(self):
    """Test filling in the payload URI of a gspaths.Payload object."""
    # Assert that it doesn't change if already present.
    pre_uri = self.mp_full_payload.uri = 'some-random-uri'
    paygen_build_lib._FillInPayloadUri(self.mp_full_payload,
                                       random_str='abc123')
    self.assertEqual(self.mp_full_payload.uri, pre_uri)

    # Test that it does change if not present.
    payload = gspaths.Payload(tgt_image=self.basic_image)
    paygen_build_lib._FillInPayloadUri(payload, random_str='abc123')
    self.assertEqual(
        payload.uri,
        'gs://crt/foo-channel/foo-board/1.2.3/payloads/'
        'chromeos_1.2.3_foo-board_foo-channel_full_mp-v2.bin-abc123.signed')

  def testFindExistingPayloads(self):
    """Test finding already existing payloads."""
    # Set up the test replay script.
    ls_mock = self.PatchObject(gs.GSContext, 'LS', return_value=['foo_result'])

    paygen = self._GetPaygenBuildInstance()
    self.assertEqual(paygen._FindExistingPayloads(self.mp_full_payload),
                     ['foo_result'])

    ls_mock.assert_called_once_with(
        'gs://crt/foo-channel/foo-board/1.2.3/payloads/'
        'chromeos_1.2.3_foo-board_foo-channel_full_mp-v2.bin-*.signed')

  def testDiscoverImages(self):
    """Test _DiscoverSignedImages."""
    paygen = self._GetPaygenBuildInstance()

    uri_base = 'gs://crt/foo-channel/foo-board/1.2.3'

    uri_basic = os.path.join(
        uri_base, 'chromeos_1.2.3_foo-board_recovery_foo-channel_mp-v3.bin')
    uri_premp = os.path.join(
        uri_base, 'chromeos_1.2.3_foo-board_recovery_foo-channel_premp.bin')

    self.mockUriList.return_value = [uri_basic, uri_premp]

    # Run the test.
    result = paygen._DiscoverSignedImages(self.target_build)

    # See if we got the results we expect.
    expected_basic = gspaths.Image(build=self.target_build, key='mp-v3',
                                   uri=uri_basic)
    expected_premp = gspaths.Image(build=self.target_build, key='premp',
                                   uri=uri_premp)
    expected_result = [expected_basic, expected_premp]

    self.assertEqual(result, expected_result)

  def testDiscoverTestImages(self):
    """Test _DiscoverTestImages (success)."""
    paygen = self._GetPaygenBuildInstance()

    uri_base = 'gs://crt/foo-channel/foo-board/1.2.3'

    uri_test_archive = os.path.join(
        uri_base, 'ChromeOS-test-R12-1.2.3-foo-board.tar.xz')
    self.mockUriList.return_value = [uri_test_archive]

    # Run the test.
    result = paygen._DiscoverTestImage(self.target_build)

    expected_test_archive = gspaths.UnsignedImageArchive(
        build=gspaths.Build(channel='foo-channel',
                            board='foo-board',
                            version='1.2.3',
                            bucket='crt'),
        uri=uri_test_archive,
        milestone='R12',
        image_type='test')

    self.assertEqual(result, expected_test_archive)

  def testDiscoverTestImagesMultipleResults(self):
    """Test _DiscoverTestImages (fails due to multiple results)."""
    paygen = self._GetPaygenBuildInstance()
    uri_base = 'gs://crt/foo-channel/foo-board/1.2.3'

    uri_test_archive1 = os.path.join(
        uri_base, 'ChromeOS-test-R12-1.2.3-foo-board.tar.xz')
    uri_test_archive2 = os.path.join(
        uri_base, 'ChromeOS-test-R13-1.2.3-foo-board.tar.xz')
    self.mockUriList.return_value = [uri_test_archive1, uri_test_archive2]

    # Run the test.
    with self.assertRaises(paygen_build_lib.BuildCorrupt):
      paygen._DiscoverTestImage(self.target_build)

  def testDiscoverRequiredDeltasBuildToBuild(self):
    """Test _DiscoverRequiredDeltasBuildToBuild"""
    paygen = self._GetPaygenBuildInstance()

    # Test the empty case.
    results = paygen._DiscoverRequiredDeltasBuildToBuild([], [])
    self.assertItemsEqual(results, [])

    # Fully populated prev and current.
    results = paygen._DiscoverRequiredDeltasBuildToBuild(
        [self.prev_test_image],
        [self.test_image])
    self.assertItemsEqual(results, [
        gspaths.Payload(src_image=self.prev_test_image,
                        tgt_image=self.test_image),
    ])

    # Mismatch MP, PreMP
    results = paygen._DiscoverRequiredDeltasBuildToBuild(
        [self.prev_premp_image],
        [self.basic_image])
    self.assertItemsEqual(results, [])

    # It's totally legal for a build to be signed for both PreMP and MP at the
    # same time. If that happens we generate:
    # MP -> MP, PreMP -> PreMP, test -> test.
    results = paygen._DiscoverRequiredDeltasBuildToBuild(
        [self.prev_image, self.prev_premp_image, self.prev_test_image],
        [self.basic_image, self.premp_image, self.test_image])
    self.assertItemsEqual(results, [
        gspaths.Payload(src_image=self.prev_image,
                        tgt_image=self.basic_image),
        gspaths.Payload(src_image=self.prev_premp_image,
                        tgt_image=self.premp_image),
        gspaths.Payload(src_image=self.prev_test_image,
                        tgt_image=self.test_image),
    ])

  def testDiscoverRequiredDLCDeltasBuildToBuild(self):
    """Test _DiscoverRequiredDLCDeltasBuildToBuild"""
    paygen = self._GetPaygenBuildInstance()

    # Test the empty case.
    results = paygen._DiscoverRequiredDLCDeltasBuildToBuild([], [])
    self.assertItemsEqual(results, [])

    # Fully populated prev and current.
    results = paygen._DiscoverRequiredDLCDeltasBuildToBuild(
        [self.prev_dlc_package_image],
        [self.dlc_image])
    self.assertItemsEqual(results, [
        gspaths.Payload(src_image=self.prev_dlc_package_image,
                        tgt_image=self.dlc_image),
    ])

    # Mismatch DLC image
    results = paygen._DiscoverRequiredDLCDeltasBuildToBuild(
        [self.prev_dlc2_image],
        [self.dlc_image])
    self.assertItemsEqual(results, [])
    results = paygen._DiscoverRequiredDLCDeltasBuildToBuild(
        [self.prev_dlc_package2_image],
        [self.dlc_image])
    self.assertItemsEqual(results, [])

  def testDiscoverDLCImages(self):
    """Test _DiscoverDLCImages."""
    paygen = self._GetPaygenBuildInstance()
    self.mockUriList.return_value = [
        ('gs://crt/foo-channel/foo-board/1.2.3/dlc/dummy-dlc/dummy-package/'
         'dlc.img')]
    dlc_module_images = paygen._DiscoverDLCImages(self.target_build)
    dlc_module_images_expected = [
        gspaths.DLCImage(
            build=self.target_build, key=None,
            uri='gs://crt/foo-channel/foo-board/1.2.3/dlc/dummy-dlc/'
                'dummy-package/dlc.img',
            dlc_id='dummy-dlc', dlc_package='dummy-package',
            dlc_image=gspaths.ChromeosReleases.DLCImageName())]
    self.assertEqual(dlc_module_images, dlc_module_images_expected)

class MockImageDiscoveryHelper(BasePaygenBuildLibTest):
  """Tests DiscoverRequiredPayloads using a fixed paygen.json from testdata."""

  def setUp(self):
    # We want to use a dict as dict key, but can't.
    # Use a list of key, value tuples.
    self.signedResults = []
    self.testResults = []
    self.dlcResults = []

    self.PatchObject(
        paygen_build_lib.PaygenBuild, '_DiscoverSignedImages',
        side_effect=self._DiscoverSignedImages)
    self.PatchObject(
        paygen_build_lib.PaygenBuild, '_DiscoverTestImage',
        side_effect=self._DiscoverTestImage)
    self.PatchObject(
        paygen_build_lib.PaygenBuild, '_DiscoverDLCImages',
        side_effect=self._DiscoverDLCImages)

  def _DiscoverSignedImages(self, build):
    for b, images in self.signedResults:
      if build == b:
        return images
    raise paygen_build_lib.ImageMissing(build)

  def _DiscoverTestImage(self, build):
    for b, images in self.testResults:
      if build == b:
        return images
    raise paygen_build_lib.ImageMissing()

  def _DiscoverDLCImages(self, build):
    for b, images in self.dlcResults:
      if build == b:
        return images
    return []

  def addSignedImage(self, build, key='mp'):
    images = []
    for i in range(len(self.signedResults)):
      if build == self.signedResults[i][0]:
        images = self.signedResults[i][1]
        self.signedResults.pop(i)
        break

    image = gspaths.Image(build=build, key=key, image_version=build.version,
                          image_type='recovery')
    images.append(image)
    self.signedResults.append((build, images))
    return image

  def addTestImage(self, build):
    for i in range(len(self.testResults)):
      if build == self.testResults[i][0]:
        self.testResults.pop(i)
        break

    image = gspaths.UnsignedImageArchive(build=build)
    self.testResults.append((build, image))
    return image

  def addDLCImage(self, build, dlc_id, dlc_package):
    image = gspaths.DLCImage(build=build, key=None, dlc_id=dlc_id,
                             dlc_package=dlc_package,
                             dlc_image=gspaths.ChromeosReleases.DLCImageName())
    self.dlcResults.append((build, [image]))
    return image

class TestPaygenBuildLibDiscoverRequiredPayloads(MockImageDiscoveryHelper,
                                                 cros_test_lib.TempDirTestCase):
  """Test deciding what payloads to generate."""

  def _GetPaygenBuildInstance(self, build, skip_delta_payloads=False):
    """Helper method to create a standard Paygen instance."""
    return paygen_build_lib.PaygenBuild(
        build, build, self.tempdir,
        config_lib_unittest.MockSiteConfig(),
        skip_delta_payloads=skip_delta_payloads)

  def testImagesNotReady(self):
    """See that we do the right thing if there are no images for the build."""
    target_build = gspaths.Build(bucket='crt',
                                 channel='canary-channel',
                                 board='auron-yuna',
                                 version='9999.0.0')

    paygen = self._GetPaygenBuildInstance(target_build)

    with self.assertRaises(paygen_build_lib.BuildNotReady):
      paygen._DiscoverRequiredPayloads()

    # Re-run with a test image, but no signed images.
    self.addTestImage(target_build)
    with self.assertRaises(paygen_build_lib.BuildNotReady):
      paygen._DiscoverRequiredPayloads()

  def testCanaryEverything(self):
    """Handle the canary payloads and tests."""
    # Make our random strings deterministic for testing.
    self.PatchObject(gspaths, '_RandomString', return_value='<random>')

    target_build = gspaths.Build(bucket='crt',
                                 channel='canary-channel',
                                 board='auron-yuna',
                                 version='9999.0.0')
    prev_build = gspaths.Build(bucket='crt',
                               channel='canary-channel',
                               board='auron-yuna',
                               version='9756.0.0')

    dlc_id = 'dummy-dlc'
    dlc_package = 'dummy-package'

    # Create our images.
    premp_image = self.addSignedImage(target_build, key='premp')
    mp_image = self.addSignedImage(target_build)
    test_image = self.addTestImage(target_build)
    prev_premp_image = self.addSignedImage(prev_build, key='premp')
    prev_mp_image = self.addSignedImage(prev_build)
    prev_test_image = self.addTestImage(prev_build)
    dlc_image = self.addDLCImage(target_build, dlc_id=dlc_id,
                                 dlc_package=dlc_package)
    prev_dlc_image = self.addDLCImage(prev_build, dlc_id=dlc_id,
                                      dlc_package=dlc_package)

    # Run the test.
    paygen = self._GetPaygenBuildInstance(target_build)
    payloads, tests = paygen._DiscoverRequiredPayloads()

    # pylint: disable=line-too-long
    # Define the expected payloads, including URLs.
    mp_full = gspaths.Payload(
        tgt_image=mp_image,
        uri='gs://crt/canary-channel/auron-yuna/9999.0.0/payloads/chromeos_9999.0.0_auron-yuna_canary-channel_full_mp.bin-<random>.signed')
    premp_full = gspaths.Payload(
        tgt_image=premp_image,
        uri='gs://crt/canary-channel/auron-yuna/9999.0.0/payloads/chromeos_9999.0.0_auron-yuna_canary-channel_full_premp.bin-<random>.signed')
    test_full = gspaths.Payload(
        tgt_image=test_image,
        uri='gs://crt/canary-channel/auron-yuna/9999.0.0/payloads/chromeos_9999.0.0_auron-yuna_canary-channel_full_test.bin-<random>')
    n2n_delta = gspaths.Payload(
        tgt_image=test_image,
        src_image=test_image,
        uri='gs://crt/canary-channel/auron-yuna/9999.0.0/payloads/chromeos_9999.0.0-9999.0.0_auron-yuna_canary-channel_delta_test.bin-<random>')
    mp_delta = gspaths.Payload(
        tgt_image=mp_image,
        src_image=prev_mp_image,
        uri='gs://crt/canary-channel/auron-yuna/9999.0.0/payloads/chromeos_9756.0.0-9999.0.0_auron-yuna_canary-channel_delta_mp.bin-<random>.signed')
    premp_delta = gspaths.Payload(
        tgt_image=premp_image,
        src_image=prev_premp_image,
        uri='gs://crt/canary-channel/auron-yuna/9999.0.0/payloads/chromeos_9756.0.0-9999.0.0_auron-yuna_canary-channel_delta_premp.bin-<random>.signed')
    test_delta = gspaths.Payload(
        tgt_image=test_image,
        src_image=prev_test_image,
        uri='gs://crt/canary-channel/auron-yuna/9999.0.0/payloads/chromeos_9756.0.0-9999.0.0_auron-yuna_canary-channel_delta_test.bin-<random>')
    dlc_full = gspaths.Payload(
        tgt_image=dlc_image,
        uri=('gs://crt/canary-channel/auron-yuna/9999.0.0/payloads/dlc/%s/%s/'
             'dlc_%s_%s_9999.0.0_auron-yuna_canary-channel_full.bin'
             '-<random>.signed' % (dlc_id, dlc_package, dlc_id, dlc_package)))
    dlc_delta = gspaths.Payload(
        tgt_image=dlc_image,
        src_image=prev_dlc_image,
        uri='gs://crt/canary-channel/auron-yuna/9999.0.0/payloads/dlc/%s/%s/'
            'dlc_%s_%s_9756.0.0-9999.0.0_auron-yuna_canary-channel_delta.bin'
            '-<random>.signed' % (dlc_id, dlc_package, dlc_id, dlc_package))

    # Verify the results.
    self.assertItemsEqual(
        payloads,
        [
            mp_full,
            premp_full,
            test_full,
            n2n_delta,
            mp_delta,
            premp_delta,
            test_delta,
            dlc_full,
            dlc_delta,
        ])

    self.assertItemsEqual(
        tests,
        [
            paygen_build_lib.PayloadTest(test_full,
                                         'canary-channel', '9999.0.0'),
            paygen_build_lib.PayloadTest(n2n_delta),
            paygen_build_lib.PayloadTest(
                test_delta,
                payload_type=paygen_build_lib.PAYLOAD_TYPE_OMAHA),
        ])

  def testCanaryPrempMismatch(self):
    """Handle the canary payloads and testss."""
    target_build = gspaths.Build(bucket='crt',
                                 channel='canary-channel',
                                 board='auron-yuna',
                                 version='9999.0.0')
    prev_build = gspaths.Build(bucket='crt',
                               channel='canary-channel',
                               board='auron-yuna',
                               version='9756.0.0')

    # Create our images.
    mp_image = self.addSignedImage(target_build)
    test_image = self.addTestImage(target_build)
    _prev_premp_image = self.addSignedImage(prev_build, key='premp')
    prev_test_image = self.addTestImage(prev_build)

    # Run the test.
    paygen = self._GetPaygenBuildInstance(target_build)
    payloads, tests = paygen._DiscoverRequiredPayloads()

    # Define the expected payloads. Test delta from prev, but no signed ones.
    mp_full = gspaths.Payload(tgt_image=mp_image, uri=mock.ANY)
    test_full = gspaths.Payload(tgt_image=test_image, uri=mock.ANY)
    n2n_delta = gspaths.Payload(tgt_image=test_image, src_image=test_image,
                                uri=mock.ANY)
    test_delta = gspaths.Payload(tgt_image=test_image,
                                 src_image=prev_test_image,
                                 uri=mock.ANY)

    # Verify the results.
    self.assertItemsEqual(
        payloads,
        [
            mp_full,
            test_full,
            n2n_delta,
            test_delta,
        ])

    self.assertItemsEqual(
        tests,
        [
            paygen_build_lib.PayloadTest(test_full,
                                         'canary-channel', '9999.0.0'),
            paygen_build_lib.PayloadTest(n2n_delta),
            paygen_build_lib.PayloadTest(
                test_delta,
                payload_type=paygen_build_lib.PAYLOAD_TYPE_OMAHA),
        ])

  def testCanarySkipDeltas(self):
    """Handle the canary payloads and testss."""
    target_build = gspaths.Build(bucket='crt',
                                 channel='canary-channel',
                                 board='auron-yuna',
                                 version='9999.0.0')
    prev_build = gspaths.Build(bucket='crt',
                               channel='canary-channel',
                               board='auron-yuna',
                               version='9756.0.0')

    # Create our images.
    mp_image = self.addSignedImage(target_build)
    test_image = self.addTestImage(target_build)
    _prev_premp_image = self.addSignedImage(prev_build, key='premp')
    _prev_test_image = self.addTestImage(prev_build)

    # Run the test.
    paygen = self._GetPaygenBuildInstance(
        target_build, skip_delta_payloads=True)
    payloads, tests = paygen._DiscoverRequiredPayloads()

    # Define the expected payloads. Test delta from prev, but no signed ones.
    mp_full = gspaths.Payload(tgt_image=mp_image, uri=mock.ANY)
    test_full = gspaths.Payload(tgt_image=test_image, uri=mock.ANY)

    # Verify the results.
    self.assertItemsEqual(
        payloads,
        [
            mp_full,
            test_full,
        ])

    self.assertItemsEqual(
        tests,
        [
            paygen_build_lib.PayloadTest(test_full,
                                         'canary-channel', '9999.0.0'),
        ])

  def testStable(self):
    """Handle the canary payloads and testss."""
    target_build = gspaths.Build(bucket='crt',
                                 channel='stable-channel',
                                 board='auron-yuna',
                                 version='9999.0.0')
    build_8530 = gspaths.Build(bucket='crt',
                               channel='stable-channel',
                               board='auron-yuna',
                               version='8530.96.0')
    build_8743 = gspaths.Build(bucket='crt',
                               channel='stable-channel',
                               board='auron-yuna',
                               version='8743.85.0')
    build_8872 = gspaths.Build(bucket='crt',
                               channel='stable-channel',
                               board='auron-yuna',
                               version='8872.76.0')
    build_9000 = gspaths.Build(bucket='crt',
                               channel='stable-channel',
                               board='auron-yuna',
                               version='9000.91.0')
    build_9202 = gspaths.Build(bucket='crt',
                               channel='stable-channel',
                               board='auron-yuna',
                               version='9202.64.0')
    build_9334 = gspaths.Build(bucket='crt',
                               channel='stable-channel',
                               board='auron-yuna',
                               version='9334.72.0')
    build_9460 = gspaths.Build(bucket='crt',
                               channel='stable-channel',
                               board='auron-yuna',
                               version='9460.60.0')
    build_9460_67 = gspaths.Build(bucket='crt',
                                  channel='stable-channel',
                                  board='auron-yuna',
                                  version='9460.67.0')

    # Create our images, ignore FSI 6457.83.0, 7390.68.0
    mp_image = self.addSignedImage(target_build)
    test_image = self.addTestImage(target_build)

    image_8530 = self.addSignedImage(build_8530)
    test_image_8530 = self.addTestImage(build_8530)

    image_8743 = self.addSignedImage(build_8743)
    test_image_8743 = self.addTestImage(build_8743)

    image_8872 = self.addSignedImage(build_8872)
    test_image_8872 = self.addTestImage(build_8872)

    image_9000 = self.addSignedImage(build_9000)
    test_image_9000 = self.addTestImage(build_9000)

    image_9202 = self.addSignedImage(build_9202)
    test_image_9202 = self.addTestImage(build_9202)

    image_9334 = self.addSignedImage(build_9334)
    test_image_9334 = self.addTestImage(build_9334)

    image_9460 = self.addSignedImage(build_9460)
    test_image_9460 = self.addTestImage(build_9460)

    image_9460_67 = self.addSignedImage(build_9460_67)
    test_image_9460_67 = self.addTestImage(build_9460_67)

    # Run the test.
    paygen = self._GetPaygenBuildInstance(target_build)
    payloads, tests = paygen._DiscoverRequiredPayloads()

    # Define the expected payloads. Test delta from prev, but no signed ones.
    mp_full = gspaths.Payload(tgt_image=mp_image, uri=mock.ANY)
    test_full = gspaths.Payload(tgt_image=test_image, uri=mock.ANY)
    n2n_delta = gspaths.Payload(tgt_image=test_image, src_image=test_image,
                                uri=mock.ANY)

    mp_delta_8530 = gspaths.Payload(
        tgt_image=mp_image, src_image=image_8530, uri=mock.ANY)
    test_delta_8530 = gspaths.Payload(
        tgt_image=test_image, src_image=test_image_8530, uri=mock.ANY)
    mp_delta_8743 = gspaths.Payload(
        tgt_image=mp_image, src_image=image_8743, uri=mock.ANY)
    test_delta_8743 = gspaths.Payload(
        tgt_image=test_image, src_image=test_image_8743, uri=mock.ANY)
    mp_delta_8872 = gspaths.Payload(
        tgt_image=mp_image, src_image=image_8872, uri=mock.ANY)
    test_delta_8872 = gspaths.Payload(
        tgt_image=test_image, src_image=test_image_8872, uri=mock.ANY)
    mp_delta_9000 = gspaths.Payload(
        tgt_image=mp_image, src_image=image_9000, uri=mock.ANY)
    test_delta_9000 = gspaths.Payload(
        tgt_image=test_image, src_image=test_image_9000, uri=mock.ANY)
    mp_delta_9202 = gspaths.Payload(
        tgt_image=mp_image, src_image=image_9202, uri=mock.ANY)
    test_delta_9202 = gspaths.Payload(
        tgt_image=test_image, src_image=test_image_9202, uri=mock.ANY)
    mp_delta_9334 = gspaths.Payload(
        tgt_image=mp_image, src_image=image_9334, uri=mock.ANY)
    test_delta_9334 = gspaths.Payload(
        tgt_image=test_image, src_image=test_image_9334, uri=mock.ANY)
    mp_delta_9460 = gspaths.Payload(
        tgt_image=mp_image, src_image=image_9460, uri=mock.ANY)
    test_delta_9460 = gspaths.Payload(
        tgt_image=test_image, src_image=test_image_9460, uri=mock.ANY)
    mp_delta_9460_67 = gspaths.Payload(
        tgt_image=mp_image, src_image=image_9460_67, uri=mock.ANY)
    test_delta_9460_67 = gspaths.Payload(
        tgt_image=test_image, src_image=test_image_9460_67, uri=mock.ANY)

    # Verify the results.
    self.assertItemsEqual(
        payloads,
        [
            mp_full,
            test_full,
            n2n_delta,
            mp_delta_8530, test_delta_8530,
            mp_delta_8743, test_delta_8743,
            mp_delta_8872, test_delta_8872,
            mp_delta_9000, test_delta_9000,
            mp_delta_9202, test_delta_9202,
            mp_delta_9334, test_delta_9334,
            mp_delta_9460, test_delta_9460,
            mp_delta_9460_67, test_delta_9460_67,
        ])

    self.assertItemsEqual(
        tests,
        [
            paygen_build_lib.PayloadTest(
                test_full, 'stable-channel', '9999.0.0'),
            paygen_build_lib.PayloadTest(
                test_full, 'stable-channel', '8530.96.0',
                payload_type=paygen_build_lib.PAYLOAD_TYPE_FSI),
            paygen_build_lib.PayloadTest(n2n_delta),
            paygen_build_lib.PayloadTest(
                test_delta_8530,
                payload_type=paygen_build_lib.PAYLOAD_TYPE_FSI),
            paygen_build_lib.PayloadTest(
                test_delta_8743,
                payload_type=paygen_build_lib.PAYLOAD_TYPE_MILESTONE),
            paygen_build_lib.PayloadTest(
                test_delta_8872,
                payload_type=paygen_build_lib.PAYLOAD_TYPE_MILESTONE),
            paygen_build_lib.PayloadTest(
                test_delta_9000,
                payload_type=paygen_build_lib.PAYLOAD_TYPE_MILESTONE),
            paygen_build_lib.PayloadTest(
                test_delta_9202,
                payload_type=paygen_build_lib.PAYLOAD_TYPE_MILESTONE),
            paygen_build_lib.PayloadTest(
                test_delta_9334,
                payload_type=paygen_build_lib.PAYLOAD_TYPE_MILESTONE),
            paygen_build_lib.PayloadTest(
                test_delta_9460,
                payload_type=paygen_build_lib.PAYLOAD_TYPE_OMAHA),
            # test_image_9460_67 had test turned off in json.
        ])


class TestPayloadGeneration(BasePaygenBuildLibTestWithBuilds):
  """Test GeneratePayloads method."""

  def testGeneratePayloads(self):
    """Test paygen_build_lib._GeneratePayloads"""
    poolMock = self.PatchObject(parallel, 'RunTasksInProcessPool')

    paygen = self._GetPaygenBuildInstance()
    paygen._GeneratePayloads((self.mp_full_payload,
                              self.mp_delta_payload,
                              self.test_delta_payload))

    self.assertEqual(
        poolMock.call_args_list,
        [mock.call(paygen_payload_lib.CreateAndUploadPayload,
                   [(self.mp_full_payload, True, True),
                    (self.mp_delta_payload, True, True),
                    (self.test_delta_payload, False, True)],
                   processes=8)])

  def testCleanupBuild(self):
    """Test PaygenBuild._CleanupBuild."""
    removeMock = self.PatchObject(gs.GSContext, 'Remove')

    paygen = self._GetPaygenBuildInstance()

    paygen._CleanupBuild()

    self.assertEqual(
        removeMock.call_args_list,
        [mock.call('gs://crt/foo-channel/foo-board/1.2.3/payloads/signing',
                   recursive=True, ignore_missing=True)])

  def testShouldSign(self):
    image = gspaths.Image()
    dlcImage = gspaths.DLCImage()
    unsignedImageArchive = gspaths.UnsignedImageArchive()
    paygen = self._GetPaygenBuildInstance()
    self.assertEqual(paygen._ShouldSign(image), True)
    self.assertEqual(paygen._ShouldSign(dlcImage), True)
    self.assertEqual(paygen._ShouldSign(unsignedImageArchive), False)


class TestCreatePayloads(BasePaygenBuildLibTestWithBuilds):
  """Test CreatePayloads."""
  def setUp(self):
    self.mockRemove = self.PatchObject(gs.GSContext, 'Remove')
    self.mockLock = self.PatchObject(gslock, 'Lock')

    self.mockDiscover = self.PatchObject(
        paygen_build_lib.PaygenBuild, '_DiscoverRequiredPayloads')
    self.mockGenerate = self.PatchObject(
        paygen_build_lib.PaygenBuild, '_GeneratePayloads')
    self.mockArchive = self.PatchObject(
        paygen_build_lib.PaygenBuild, '_MapToArchive')
    self.mockAutotest = self.PatchObject(
        paygen_build_lib.PaygenBuild, '_AutotestPayloads')
    self.mockCleanup = self.PatchObject(
        paygen_build_lib.PaygenBuild, '_CleanupBuild')
    self.mockExisting = self.PatchObject(
        paygen_build_lib.PaygenBuild, '_FindExistingPayloads')

  def testCreatePayloadsLockedBuild(self):
    self.mockLock.side_effect = gslock.LockNotAcquired

    paygen = self._GetPaygenBuildInstance()

    with self.assertRaises(paygen_build_lib.BuildLocked):
      paygen.CreatePayloads()

  def testCreatePayloadsBuildNotReady(self):
    """Test paygen_build_lib._GeneratePayloads if not all images are there."""
    self.mockDiscover.side_effect = paygen_build_lib.BuildNotReady

    paygen = self._GetPaygenBuildInstance()

    with self.assertRaises(paygen_build_lib.BuildNotReady):
      paygen.CreatePayloads()

    self.assertEqual(self.mockCleanup.call_args_list, [mock.call()])

  def testCreatePayloadsCreateFailed(self):
    """Test paygen_build_lib._GeneratePayloads if payload generation failed."""
    mockException = Exception

    self.mockGenerate.side_effect = mockException

    paygen = self._GetPaygenBuildInstance()

    with self.assertRaises(mockException):
      paygen.CreatePayloads()

    self.assertEqual(self.mockCleanup.call_args_list, [mock.call()])

  def testCreatePayloadsSuccess(self):
    """Test paygen_build_lib._GeneratePayloads success."""
    self.mockArchive.return_value = (
        'archive_board', 'archive_build', 'archive_build_uri')
    self.mockAutotest.return_value = 'suite_name'
    self.mockExisting.return_value = None

    payloads = [
        self.mp_full_payload,
        self.test_full_payload,
        self.test_delta_payload,
    ]
    payload_tests = [
        self.full_payload_test,
        self.delta_payload_test,
    ]

    self.mockDiscover.return_value = (payloads, payload_tests)

    paygen = self._GetPaygenBuildInstance()

    testdata = paygen.CreatePayloads()

    self.assertEqual(
        testdata,
        ('suite_name', 'archive_board', 'archive_build', [])
    )

    self.assertEqual(self.mockGenerate.call_args_list, [
        mock.call(payloads),
    ])

    self.assertEqual(self.mockArchive.call_args_list, [
        mock.call('foo-board', '1.2.3'),
    ])

    self.assertEqual(self.mockAutotest.call_args_list, [
        mock.call(payload_tests),
    ])

    self.assertEqual(self.mockCleanup.call_args_list, [mock.call()])

  def testCreatePayloadsSuccessAllExist(self):
    """Test paygen_build_lib._GeneratePayloads success."""
    self.mockArchive.return_value = (
        'archive_board', 'archive_build', 'archive_build_uri')
    self.mockAutotest.return_value = 'suite_name'
    self.mockExisting.return_value = ['existing_url']

    payloads = [
        self.mp_full_payload,
        self.test_full_payload,
        self.test_delta_payload,
    ]
    payload_tests = [
        self.full_payload_test,
        self.delta_payload_test,
    ]

    self.mockDiscover.return_value = (payloads, payload_tests)

    paygen = self._GetPaygenBuildInstance()

    testdata = paygen.CreatePayloads()

    self.assertEqual(
        testdata,
        ('suite_name', 'archive_board', 'archive_build', [])
    )

    # Note... no payloads were generated.
    self.assertEqual(self.mockGenerate.call_args_list, [])

    self.assertEqual(self.mockArchive.call_args_list, [
        mock.call('foo-board', '1.2.3'),
    ])

    self.assertEqual(self.mockAutotest.call_args_list, [
        mock.call(payload_tests),
    ])

    self.assertEqual(self.mockCleanup.call_args_list, [mock.call()])


class TestAutotestPayloadsPayloads(BasePaygenBuildLibTestWithBuilds):
  """Test autotest tarball generation."""
  def setUp(self):
    # For autotest, we have to look up URLs for old builds. Mock out lookup.
    self.mockFind = self.PatchObject(
        paygen_build_lib.PaygenBuild, '_FindFullTestPayloads',
        side_effect=lambda channel, version: ['%s_%s_uri' % (channel, version)])

    self.mockExists = self.PatchObject(
        gs.GSContext, 'Exists',
        side_effect=lambda uri: uri and uri.endswith('stateful.tgz'))

    self.mockCopy = self.PatchObject(gs.GSContext, 'Copy')

    # Our images have to exist, and have URIs for autotest.
    self.test_image.uri = 'test_image_uri'
    self.prev_test_image.uri = 'prev_test_image_uri'

  def testAutotestPayloadsSuccess(self):
    payload_tests = [
        self.full_payload_test,
        self.delta_payload_test,
    ]

    paygen = self._GetPaygenBuildInstance()

    # These are normally set during CreatePayloads.
    paygen._archive_board = 'archive_board'
    paygen._archive_build = 'archive_board/R62-9778.0.0'
    paygen._archive_build_uri = 'archive_uri'

    paygen._AutotestPayloads(payload_tests)

    tarball_path = os.path.join(
        self.tempdir, 'autotests/paygen_au_foo_control.tar.bz2')

    # Verify that we uploaded the results correctly.
    self.assertEqual(self.mockCopy.call_args_list, [
        mock.call(tarball_path, 'archive_uri/paygen_au_foo_control.tar.bz2',
                  acl='public-read'),
    ])

    delta_ctrl = (
        'autotest/au_control_files/control.paygen_au_foo_delta_1.0.0_omaha')
    full_ctrl = (
        'autotest/au_control_files/control.paygen_au_foo_full_1.2.3_n2n')

    # Verify tarfile contents.
    with tarfile.open(tarball_path) as t:
      self.assertItemsEqual(
          t.getnames(),
          ['autotest/au_control_files', delta_ctrl, full_ctrl])

      delta_fp = t.extractfile(delta_ctrl)
      delta_contents = delta_fp.read()
      delta_fp.close()

      full_fp = t.extractfile(full_ctrl)
      full_contents = full_fp.read()
      full_fp.close()

    # We only checking the beginning to avoid the very long doc string.
    self.assertTrue(delta_contents.startswith("""name = 'paygen_au_foo'
update_type = 'delta'
source_release = '1.0.0'
target_release = '1.2.3'
target_payload_uri = 'None'
SUITE = 'paygen_au_foo'
source_payload_uri = 'foo-channel_1.0.0_uri'
source_archive_uri = 'gs://crt/foo-channel/foo-board/1.0.0'
payload_type = 'OMAHA'
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
from autotest_lib.client.bin import sysinfo
from autotest_lib.client.common_lib import error, utils
from autotest_lib.client.cros import constants
from autotest_lib.server import host_attributes

AUTHOR = "Chromium OS"
NAME = "autoupdate_EndToEndTest_paygen_au_foo_delta_1.0.0_omaha"
TIME = "MEDIUM"
TEST_CATEGORY = "Functional"
TEST_CLASS = "platform"
TEST_TYPE = "server"
JOB_RETRIES = 1
BUG_TEMPLATE = {
    'cc': ['chromeos-installer-alerts@google.com'],
    'components': ['Internals>Installer'],
}

# Skip provision special task for AU tests.
DEPENDENCIES = "skip_provision"

# Disable server-side packaging support for this test.
# This control file is used as the template for paygen_au_canary suite, which
# creates the control files during paygen. Therefore, autotest server package
# does not have these test control files for paygen_au_canary suite.
REQUIRE_SSP = False

DOC ="""))

    # We only checking the beginning to avoid the very long doc string.
    self.assertTrue(full_contents.startswith("""name = 'paygen_au_foo'
update_type = 'full'
source_release = '1.2.3'
target_release = '1.2.3'
target_payload_uri = 'None'
SUITE = 'paygen_au_foo'
source_payload_uri = 'foo-channel_1.2.3_uri'
source_archive_uri = 'gs://crt/foo-channel/foo-board/1.2.3'
payload_type = 'N2N'
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
from autotest_lib.client.bin import sysinfo
from autotest_lib.client.common_lib import error, utils
from autotest_lib.client.cros import constants
from autotest_lib.server import host_attributes

AUTHOR = "Chromium OS"
NAME = "autoupdate_EndToEndTest_paygen_au_foo_full_1.2.3_n2n"
TIME = "MEDIUM"
TEST_CATEGORY = "Functional"
TEST_CLASS = "platform"
TEST_TYPE = "server"
JOB_RETRIES = 1
BUG_TEMPLATE = {
    'cc': ['chromeos-installer-alerts@google.com'],
    'components': ['Internals>Installer'],
}

# Skip provision special task for AU tests.
DEPENDENCIES = "skip_provision"

# Disable server-side packaging support for this test.
# This control file is used as the template for paygen_au_canary suite, which
# creates the control files during paygen. Therefore, autotest server package
# does not have these test control files for paygen_au_canary suite.
REQUIRE_SSP = False

DOC ="""))


class HWTest(cros_test_lib.MockTestCase):
  """Test HW test invocation."""

  def testScheduleAutotestTests(self):
    run_test_mock = self.PatchObject(
        commands,
        'RunSkylabHWTestPlan',
        return_value=commands.HWTestSuiteResult(None, None))

    paygen_build_lib.ScheduleAutotestTests(
        suite_name='dummy-suite',
        board='dummy-board',
        model='dummy-model',
        build='dummy-build',
        skip_duts_check=None, # ignored
        debug=None, # ignored
        payload_test_configs=[],
        test_env=None, # ignored
        job_keyvals=None) # ignored

    run_test_mock.assert_called_once_with(
        test_plan=paygen_build_lib._TestPlan(
            payload_test_configs=[],
            suite_name='dummy-suite',
            build='dummy-build'),
        build='dummy-build',
        legacy_suite='dummy-suite',
        pool='DUT_POOL_BVT',
        board='dummy-board',
        model='dummy-model',
        timeout_mins=2*3*60,
        tags=['build:dummy-build',
              'suite:dummy-suite',
              'user:PaygenTestStage'],
        keyvals={'build': 'dummy-build',
                 'suite': 'dummy-suite'})

  def testTestPlan(self):
    payload_test_configs = [
        test_params.TestConfig(
            board='dummy-board',
            name='dummy-test',
            is_delta_update=True,
            source_release='source-release',
            target_release='target-release',
            source_payload_uri='source-uri',
            target_payload_uri='target-uri',
            suite_name='dummy-suite')]

    test_plan_string = paygen_build_lib._TestPlan(
        payload_test_configs=payload_test_configs,
        suite_name='dummy-suite',
        build='dummy-build')
    test_plan_dict = json.loads(test_plan_string)
    test_args_string = test_plan_dict[
        'enumeration']['autotestInvocations'][0].pop('testArgs')
    # There's no guarantee on the order.
    test_args_set = set(test_args_string.split(' '))

    expected_test_plan = {
        'enumeration': {
            'autotestInvocations': [{
                'test': {
                    'name': 'autoupdate_EndToEndTest',
                    'allowRetries': True,
                    'maxRetries': 1,
                    'executionEnvironment': 'EXECUTION_ENVIRONMENT_SERVER'
                },
                'displayName': 'dummy-build/dummy-suite/' +
                               'autoupdate_EndToEndTest_' +
                               'dummy-test_delta_source-release'
            }]
        }
    }
    expected_test_args = set(['name=dummy-test',
                              'update_type=delta',
                              'source_release=source-release',
                              'target_release=target-release',
                              'target_payload_uri=target-uri',
                              'SUITE=dummy-suite',
                              'source_payload_uri=source-uri'])

    self.assertDictEqual(test_plan_dict, expected_test_plan)
    self.assertSetEqual(test_args_set, expected_test_args)
