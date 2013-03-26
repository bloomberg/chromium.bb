#!/usr/bin/python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module tests the cros image command."""

import copy
import mock
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
  def testNormal(self):
    """Tests that our example parser works normally."""
    with MockChromeSDKCommand(
        ['--board', SDKFetcherMock.BOARD],
        base_args=['--cache-dir', self.tempdir]) as bootstrap:
      self.assertEquals(bootstrap.inst.options.board, SDKFetcherMock.BOARD)
      self.assertEquals(bootstrap.inst.options.cache_dir, self.tempdir)


def _GSCopyMock(_self, path, dest):
  """Used to simulate a GS Copy operation."""
  with osutils.TempDir() as tempdir:
    local_path = os.path.join(tempdir, os.path.basename(path))
    osutils.Touch(local_path)
    shutil.move(local_path, dest)


def _DependencyMockCtx(f):
  """Attribute that ensures dependency PartialMocks are started.

  Since PartialMock does not support nested mocking, we need to first call
  stop() on the outer level PartialMock (which is passed in to us).  We then
  re-start() the outer level upon exiting the context.
  """
  def new_f(self, *args, **kwargs):
    if not self.entered:
      try:
        self.entered = True
        # Temporarily disable outer GSContext mock before starting our mock.
        # TODO(rcui): Generalize this attribute and include in partial_mock.py.
        if self.ext_gs_mock:
          self.ext_gs_mock.stop()

        with self.gs_mock:
          return f(self, *args, **kwargs)
      finally:
        self.entered = False
        if self.ext_gs_mock:
          self.ext_gs_mock.start()
    else:
      return f(self, *args, **kwargs)
  return new_f


class SDKFetcherMock(partial_mock.PartialMock):
  """Provides mocking functionality for SDKFetcher."""

  TARGET = 'chromite.cros.commands.cros_chrome_sdk.SDKFetcher'
  ATTRS = ('__init__', '_GetChromeLKGM', '_GetNewestManifestVersion',
           '_UpdateTarball', '_GetMetadata')

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

  BOARD = 'x86-alex'
  VERSION = 'R25-3543.2.0'

  def __init__(self, gs_mock=None):
    """Initializes the mock.

    Arguments:
      gs_mock: An already started GSContextMock instance.  stop() will be called
        on this instance every time execution enters one our the mocked out
        methods, and start() called on it once execution leaves the mocked out
        method.
    """
    partial_mock.PartialMock.__init__(self)
    self.ext_gs_mock = gs_mock
    self.entered = False
    self.gs_mock = gs_unittest.GSContextMock()
    self.gs_mock.SetDefaultCmdResult()

  @_DependencyMockCtx
  def _target__init__(self, inst, *args, **kwargs):
    self.backup['__init__'](inst, *args, **kwargs)
    if not inst.cache_base.startswith('/tmp'):
      raise AssertionError('For testing, SDKFetcher cache_dir needs to be a '
                           'dir under /tmp')

  @_DependencyMockCtx
  def _GetChromeLKGM(self, _inst, *_args, **_kwargs):
    return None

  @_DependencyMockCtx
  def _GetNewestManifestVersion(self, _inst, *_args, **_kwargs):
    return self.VERSION

  @_DependencyMockCtx
  def _UpdateTarball(self, inst, *args, **kwargs):
    with mock.patch.object(gs.GSContext, 'Copy', autospec=True,
                           side_effect=_GSCopyMock):
      with mock.patch.object(cache, 'Untar'):
        return self.backup['_UpdateTarball'](inst, *args, **kwargs)

  @_DependencyMockCtx
  def _GetMetadata(self, inst, *args, **kwargs):
    self.gs_mock.SetDefaultCmdResult()
    self.gs_mock.AddCmdResult(
        partial_mock.ListRegex('cat .*/%s' % constants.METADATA_JSON),
        output=self.FAKE_METADATA)
    return self.backup['_GetMetadata'](inst, *args, **kwargs)


class RunThroughTest(cros_test_lib.MockTempDirTestCase):
  """Run the script with most things mocked out."""

  VERSION_KEY = (SDKFetcherMock.BOARD, SDKFetcherMock.VERSION,
                 constants.CHROME_SYSROOT_TAR)

  FAKE_ENV = {
      'GYP_DEFINES': "sysroot='/path/to/sysroot'",
      'CXX': 'x86_64-cros-linux-gnu-g++ -B /path/to/gold',
      'CC': 'x86_64-cros-linux-gnu-gcc -B /path/to/gold',
      'LD': 'x86_64-cros-linux-gnu-g++ -B /path/to/gold',
  }

  def setUp(self):
    self.sdk_mock = self.StartPatcher(SDKFetcherMock())
    self.cmd_mock = MockChromeSDKCommand(
        ['--board', SDKFetcherMock.BOARD, 'true'],
        base_args=['--cache-dir', self.tempdir])
    self.StartPatcher(self.cmd_mock)
    self.cmd_mock.UnMockAttr('Run')

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
    """Tests that SDKFetcher.Prepare() handles |components| param properly."""
    sdk = cros_chrome_sdk.SDKFetcher(os.path.join(self.tempdir),
                                    SDKFetcherMock.BOARD)
    components = [constants.BASE_IMAGE_TAR, constants.CHROME_SYSROOT_TAR]
    with sdk.Prepare(components=components) as ctx:
      for c in components:
        self.assertTrue(os.path.exists(ctx.key_map[c].path))
      for c in [constants.IMAGE_SCRIPTS_TAR, constants.CHROME_ENV_TAR]:
        self.assertFalse(c in ctx.key_map)


class VersionTest(cros_test_lib.MockTempDirTestCase):
  """Tests the determination of which SDK version to use."""

  PARTIAL_VERSION = '3543.2.0'
  VERSION = 'RXX-%s' % PARTIAL_VERSION
  ENV_VERSION = 'RYY-%s' % PARTIAL_VERSION
  BOARD = 'lumpy'

  VERSION_BASE = ("gs://chromeos-image-archive/%s-release/%s"
                  % (BOARD, VERSION))
  FAKE_LS = """\
%(base)s/UPLOADED
%(base)s/au-generator.zip
%(base)s/autotest.tar
%(base)s/bootloader.tar.xz
""" % {'base': VERSION_BASE}

  def setUp(self):
    self.gs_mock = self.StartPatcher(gs_unittest.GSContextMock())
    self.gs_mock.SetDefaultCmdResult()
    self.sdk_mock = self.StartPatcher(SDKFetcherMock(gs_mock=self.gs_mock))
    self.sdk_mock.UnMockAttr('_GetChromeLKGM')

    os.environ.pop(cros_chrome_sdk.SDKFetcher.SDK_VERSION_ENV, None)
    self.sdk = cros_chrome_sdk.SDKFetcher(
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
    # pylint: disable=E1101
    self.assertTrue(gclient.FindGclientCheckoutRoot.called)

  def testDefaultEnvBadBoard(self):
    """We don't use the version in the environment if board doesn't match."""
    os.environ[cros_chrome_sdk.SDKFetcher.SDK_VERSION_ENV] = self.ENV_VERSION
    self.assertEquals(self.sdk.GetDefaultVersion(), self.sdk_mock.VERSION)

  def testDefaultEnvGoodBoard(self):
    """We use the version in the environment if board matches."""
    os.environ[cros_chrome_sdk.SDKFetcher.SDK_VERSION_ENV] = self.ENV_VERSION
    os.environ[cros_chrome_sdk.SDKFetcher.SDK_BOARD_ENV] = self.BOARD
    self.assertEquals(self.sdk.GetDefaultVersion(), self.ENV_VERSION)


if __name__ == '__main__':
  cros_test_lib.main()
