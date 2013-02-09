#!/usr/bin/python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module tests the cros image command."""

import copy
import os
import shutil
import sys

sys.path.insert(0, os.path.abspath('%s/../../..' % os.path.dirname(__file__)))
from chromite.buildbot import constants
from chromite.cros.commands import cros_chrome_sdk
from chromite.cros.commands import init_unittest
from chromite.lib import cache
from chromite.lib import cros_test_lib
from chromite.lib import gclient
from chromite.lib import gs
from chromite.lib import gs_unittest
from chromite.lib import osutils
from chromite.lib import partial_mock


class MockChromeSDKCommand(init_unittest.MockCommand):
  """Mock out the build command."""
  TARGET = 'chromite.cros.commands.cros_chrome_sdk.ChromeSDKCommand'
  TARGET_CLASS = cros_chrome_sdk.ChromeSDKCommand
  COMMAND = 'chrome-sdk'


class ParserTest(cros_test_lib.MockTempDirTestCase):
  """Test the parser."""

  BOARD = 'lumpy'

  def testNormal(self):
    """Tests that our example parser works normally."""
    with MockChromeSDKCommand(
        ['--board', self.BOARD],
        base_args=['--cache-dir', self.tempdir]) as bootstrap:
      self.assertEquals(bootstrap.inst.options.board, self.BOARD)
      self.assertEquals(bootstrap.inst.options.cache_dir, self.tempdir)


def _GSCopyMock(_self, path, dest):
  """Used to simulate a GS Copy operation."""
  with osutils.TempDirContextManager() as tempdir:
    local_path = os.path.join(tempdir, os.path.basename(path))
    osutils.Touch(local_path)
    shutil.move(local_path, dest)


class ChromeSDKTest(cros_test_lib.MockTempDirTestCase):
  """Base class for SDK tests.  Contains shared functionality."""
  BOARD = 'lumpy'
  PARTIAL_VERSION = '3543.2.0'
  VERSION = 'R25-%s' % PARTIAL_VERSION

  def setUp(self):
    self.gs_mock = gs_unittest.GSContextMock()
    self.gs_mock.SetDefaultCmdResult()
    self.StartPatcher(self.gs_mock)


class RunThroughTest(ChromeSDKTest):
  """Run the script with most things mocked out."""

  VERSION_KEY = (ChromeSDKTest.BOARD, ChromeSDKTest.VERSION,
                 constants.CHROME_SYSROOT_TAR)
  FAKE_METADATA = """
{
  "boards": ["x86-alex"],
  "cros-version": "25.3543.2",
  "metadata-version": "1",
  "bot-hostname": "build82-m2.golo.chromium.org",
  "bot-config": "x86-alex-release",
  "toolchain-tuple": ["i686-pc-linux-gnu"],
  "toolchain-url": "2013/01/%(target)s-2013.01.23.003823.tar.xz",
  "sdk-version": "2013.01.23.003823"
}"""

  FAKE_ENV = {
      'GYP_DEFINES': "sysroot='/path/to/sysroot'",
      'CXX': 'x86_64-cros-linux-gnu-g++ -B /path/to/gold',
      'CC': 'x86_64-cros-linux-gnu-gcc -B /path/to/gold',
      'LD': 'x86_64-cros-linux-gnu-g++ -B /path/to/gold',
  }

  def setUp(self):
    self.gs_mock.AddCmdResult(partial_mock.ListRegex('cat .*/LATEST.*'),
                              output=self.VERSION)
    self.gs_mock.AddCmdResult(
        partial_mock.ListRegex('cat .*/%s' % constants.METADATA_JSON),
        output=self.FAKE_METADATA)

    self.cmd_mock = MockChromeSDKCommand(
        ['--board', self.BOARD, 'true'],
        base_args=['--cache-dir', self.tempdir])
    self.StartPatcher(self.cmd_mock)
    self.cmd_mock.UnMockAttr('Run')

    self.untar_mock = self.PatchObject(cache, 'Untar')
    self.PatchObject(gs.GSContext, 'Copy',
                     autospec=True, side_effect=_GSCopyMock)
    self.PatchObject(osutils, 'SourceEnvironment',
                     autospec=True, return_value=copy.copy(self.FAKE_ENV))

  @property
  def cache(self):
    return self.cmd_mock.inst.sdk.tarball_cache

  def testIt(self):
    """Test a runthrough of the script."""
    self.cmd_mock.inst.Run()
    with self.cache.Lookup(self.VERSION_KEY) as r:
      self.assertTrue(r.Exists())

  def testSpecificComponent(self):
    """Tests that ChromeSDK.Prepare() handles |components| param properly."""
    sdk = cros_chrome_sdk.ChromeSDK(os.path.join(self.tempdir), self.BOARD)
    components = [constants.BASE_IMAGE_TAR, constants.CHROME_SYSROOT_TAR]
    with sdk.Prepare(version=self.VERSION,
                     components=components) as ctx:
      for c in components:
        self.assertTrue(os.path.exists(ctx.key_map[c].path))
      for c in [constants.IMAGE_SCRIPTS_TAR, constants.CHROME_ENV_TAR]:
        self.assertFalse(c in ctx.key_map)


class VersionTest(ChromeSDKTest):
  """Tests the determination of which SDK version to use."""

  ENV_VERSION = 'R26-1234.0.0'

  VERSION_BASE = ("gs://chromeos-image-archive/%s-release/%s"
                  % (ChromeSDKTest.BOARD, ChromeSDKTest.VERSION))
  FAKE_LS = """\
%(base)s/UPLOADED
%(base)s/au-generator.zip
%(base)s/autotest.tar
%(base)s/bootloader.tar.xz
""" % {'base': VERSION_BASE}

  def setUp(self):
    os.environ.pop(cros_chrome_sdk.SDK_VERSION_ENV, None)
    self.sdk = cros_chrome_sdk.ChromeSDK(
        os.path.join(self.tempdir, 'cache'), self.BOARD)

  def testChromeVersion(self):
    """We pick up the right LKGM version from the Chrome tree."""
    gclient_root = os.path.join(self.tempdir, 'gclient_root')
    self.PatchObject(gclient, 'FindGclientCheckoutRoot',
                     return_value=gclient_root)

    self.gs_mock.AddCmdResult(
        partial_mock.ListRegex('ls .*-%s' % self.PARTIAL_VERSION),
        output=self.FAKE_LS)

    lkgm_file = os.path.join(gclient_root, constants.PATH_TO_CHROME_LKGM)
    osutils.Touch(lkgm_file, makedirs=True)
    osutils.WriteFile(lkgm_file, self.PARTIAL_VERSION)
    self.sdk.UpdateDefaultVersion()
    self.assertEquals(self.sdk.GetDefaultVersion(),
                      self.VERSION)

  def testDefaultEnv(self):
    """Test that we pick up the version from the environment."""
    os.environ[cros_chrome_sdk.SDK_VERSION_ENV] = self.ENV_VERSION
    self.assertEquals(self.sdk.GetDefaultVersion(), self.ENV_VERSION)


if __name__ == '__main__':
  cros_test_lib.main()
