# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Collection of tests to run on the rootfs of a built image."""

import itertools
import os
import unittest

from chromite.lib import cros_build_lib
from chromite.lib import osutils
import lddtree


ROOT_A = 'dir-ROOT-A'
STATEFUL = 'dir-STATE'


class BoardAndDirectoryMixin(object):
  """A mixin to hold image test's specific info."""

  _board = None
  _result_dir = None

  def SetBoard(self, board):
    self._board = board

  def SetResultDir(self, result_dir):
    self._result_dir = result_dir


class ImageTestCase(unittest.TestCase, BoardAndDirectoryMixin):
  """Subclass unittest.TestCase to provide utility methods for image tests.

  Tests should not directly inherit this class. They should instead inherit
  from ForgivingImageTestCase, or NonForgivingImageTestCase.

  Tests MUST use prefix "Test" (e.g.: TestLinkage, TestDiskSpace), not "test"
  prefix, in order to be picked up by the test runner.

  Tests are run outside chroot.

  The current working directory is set up so that "ROOT_A", and "STATEFUL"
  constants refer to the mounted partitions. The partitions are mounted
  readonly.

    current working directory
      + ROOT_A
        + /
          + bin
          + etc
          + usr
          ...
      + STATEFUL
        + var_overlay
        ...
  """

  def IsForgiving(self):
    """Indicate if this test is forgiving.

    The test runner will classify tests into two buckets, forgiving and non-
    forgiving. Forgiving tests DO NOT affect the result of the test runner;
    non-forgiving tests do. In either case, test runner will still output the
    result of each individual test.
    """
    raise NotImplementedError()


class ForgivingImageTestCase(ImageTestCase):
  """Concrete base class of forgiving tests."""

  def IsForgiving(self):
    return True


class NonForgivingImageTestCase(ImageTestCase):
  """Concrete base class of non forgiving tests."""

  def IsForgiving(self):
    return False


class ImageTestSuite(unittest.TestSuite, BoardAndDirectoryMixin):
  """Wrap around unittest.TestSuite to pass more info to the actual tests."""

  def GetTests(self):
    return self._tests

  def run(self, result, debug=False):
    for t in self._tests:
      t.SetResultDir(self._result_dir)
      t.SetBoard(self._board)
    return super(ImageTestSuite, self).run(result)


class ImageTestRunner(unittest.TextTestRunner, BoardAndDirectoryMixin):
  """Wrap around unittest.TextTestRunner to pass more info down the chain."""

  def run(self, test):
    test.SetResultDir(self._result_dir)
    test.SetBoard(self._board)
    return super(ImageTestRunner, self).run(test)


#####################
# Here go the tests
#####################


class LocaltimeTest(NonForgivingImageTestCase):
  """Verify that /etc/localtime is a symlink to /var/lib/timezone/localtime.

  This is an example of an image test. The image is already mounted. The
  test can access rootfs via ROOT_A constant.
  """

  def TestLocaltimeIsSymlink(self):
    localtime_path = os.path.join(ROOT_A, 'etc', 'localtime')
    self.assertTrue(os.path.islink(localtime_path))

  def TestLocaltimeLinkIsCorrect(self):
    localtime_path = os.path.join(ROOT_A, 'etc', 'localtime')
    self.assertEqual('/var/lib/timezone/localtime',
                     os.readlink(localtime_path))


class BlacklistTest(NonForgivingImageTestCase):
  """Verify that rootfs does not contain blacklisted directories."""

  def TestBlacklistedDirectories(self):
    dirs = [os.path.join(ROOT_A, 'usr', 'share', 'locale')]
    for d in dirs:
      self.assertFalse(os.path.isdir(d), 'Directory %s is blacklisted.' % d)


class LinkageTest(NonForgivingImageTestCase):
  """Verify that all binaries and libraries have proper linkage."""

  def setUp(self):
    self._outside_chroot = os.getcwd()
    try:
      self._inside_chroot = cros_build_lib.ToChrootPath(self._outside_chroot)
    except ValueError:
      self._inside_chroot = self._outside_chroot

    osutils.MountDir(
        os.path.join(self._outside_chroot, STATEFUL, 'var_overlay'),
        os.path.join(self._outside_chroot, ROOT_A, 'var'),
        mount_opts=('bind', ),
    )

  def tearDown(self):
    osutils.UmountDir(
        os.path.join(self._outside_chroot, ROOT_A, 'var'),
        cleanup=False,
    )

  def _IsPackageMerged(self, package_name):
    cmd = [
        'portageq-%s' % self._board,
        'has_version',
        os.path.join(self._inside_chroot, ROOT_A),
        package_name
    ]
    ret = cros_build_lib.RunCommand(cmd, error_code_ok=True,
                                    enter_chroot=True)
    return ret.returncode == 0

  def TestLinkage(self):
    """Find main executable binaries and check their linkage."""
    binaries = [
        'boot/vmlinuz',
        'bin/sed',
    ]

    if self._IsPackageMerged('chromeos-base/chromeos-login'):
      binaries.append('sbin/session_manager')

    if self._IsPackageMerged('x11-base/xorg-server'):
      binaries.append('usr/bin/Xorg')

    # When chrome is built with USE="pgo_generate", rootfs chrome is actually a
    # symlink to a real binary which is in the stateful partition. So we do not
    # check for a valid chrome binary in that case.
    if (not self._IsPackageMerged('chromeos-base/chromeos-chrome[pgo_generate]')
        and self._IsPackageMerged('chromeos-base/chromeos-chrome')):
      binaries.append('opt/google/chrome/chrome')

    binaries = [os.path.join(ROOT_A, x) for x in binaries]

    # Grab all .so files
    libraries = []
    for root, _, files in os.walk(ROOT_A):
      for name in files:
        filename = os.path.join(root, name)
        if '.so' in filename:
          libraries.append(filename)

    ldpaths = lddtree.LoadLdpaths(ROOT_A)
    for to_test in itertools.chain(binaries, libraries):
      try:
        lddtree.ParseELF(to_test, ROOT_A, ldpaths)
      except lddtree.exceptions.ELFError:
        continue
      except IOError as e:
        self.fail(e)
