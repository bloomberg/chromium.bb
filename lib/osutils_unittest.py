# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for the osutils.py module (imagine that!)."""

from __future__ import print_function

import mock
import os

from chromite.lib import cros_build_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import partial_mock


class TestOsutils(cros_test_lib.TempDirTestCase):
  """General unittests for the osutils module."""

  def testReadWriteFile(self):
    """Verify we can write data to a file, and then read it back."""
    filename = os.path.join(self.tempdir, 'foo')
    data = 'alsdkfjasldkfjaskdlfjasdf'
    self.assertEqual(osutils.WriteFile(filename, data), None)
    self.assertEqual(osutils.ReadFile(filename), data)

  def testSafeUnlink(self):
    """Test unlinking files work (existing or not)."""
    def f(dirname, sudo=False):
      dirname = os.path.join(self.tempdir, dirname)
      path = os.path.join(dirname, 'foon')
      os.makedirs(dirname)
      open(path, 'w').close()
      self.assertTrue(os.path.exists(path))
      if sudo:
        cros_build_lib.SudoRunCommand(
            ['chown', 'root:root', '-R', '--', dirname], print_cmd=False)
        self.assertRaises(EnvironmentError, os.unlink, path)
      self.assertTrue(osutils.SafeUnlink(path, sudo=sudo))
      self.assertFalse(os.path.exists(path))
      self.assertFalse(osutils.SafeUnlink(path))
      self.assertFalse(os.path.exists(path))

    f("nonsudo", False)
    f("sudo", True)

  def testSafeMakedirs(self):
    """Test creating directory trees work (existing or not)."""
    path = os.path.join(self.tempdir, 'a', 'b', 'c', 'd', 'e')
    self.assertTrue(osutils.SafeMakedirs(path))
    self.assertTrue(os.path.exists(path))
    self.assertFalse(osutils.SafeMakedirs(path))
    self.assertTrue(os.path.exists(path))

  def testSafeMakedirs_error(self):
    """Check error paths."""
    self.assertRaises(OSError, osutils.SafeMakedirs, '/foo/bar/cow/moo/wee')
    self.assertRaises(OSError, osutils.SafeMakedirs, '')

  def testSafeMakedirsSudo(self):
    """Test creating directory trees work as root (existing or not)."""
    path = os.path.join(self.tempdir, 'a', 'b', 'c', 'd', 'e')
    self.assertTrue(osutils.SafeMakedirs(path, sudo=True))
    self.assertTrue(os.path.exists(path))
    self.assertFalse(osutils.SafeMakedirs(path, sudo=True))
    self.assertTrue(os.path.exists(path))
    self.assertEqual(os.stat(path).st_uid, 0)
    # Have to manually clean up as a non-root `rm -rf` will fail.
    cros_build_lib.SudoRunCommand(['rm', '-rf', self.tempdir], print_cmd=False)

  def testRmDir(self):
    """Test that removing dirs work."""
    path = os.path.join(self.tempdir, 'a', 'b', 'c', 'd', 'e')

    self.assertRaises(EnvironmentError, osutils.RmDir, path)
    osutils.SafeMakedirs(path)
    osutils.RmDir(path)
    osutils.RmDir(path, ignore_missing=True)
    self.assertRaises(EnvironmentError, osutils.RmDir, path)

    osutils.SafeMakedirs(path)
    osutils.RmDir(path)
    self.assertFalse(os.path.exists(path))

  def testRmDirSudo(self):
    """Test that removing dirs via sudo works."""
    subpath = os.path.join(self.tempdir, 'a')
    path = os.path.join(subpath, 'b', 'c', 'd', 'e')
    self.assertTrue(osutils.SafeMakedirs(path, sudo=True))
    self.assertRaises(OSError, osutils.RmDir, path)
    osutils.RmDir(subpath, sudo=True)
    self.assertRaises(cros_build_lib.RunCommandError,
                      osutils.RmDir, subpath, sudo=True)

  def testTouchFile(self):
    """Test that we can touch files."""
    path = os.path.join(self.tempdir, 'touchit')
    self.assertFalse(os.path.exists(path))
    osutils.Touch(path)
    self.assertTrue(os.path.exists(path))
    self.assertEqual(os.path.getsize(path), 0)

  def testTouchFileSubDir(self):
    """Test that we can touch files in non-existent subdirs."""
    path = os.path.join(self.tempdir, 'a', 'b', 'c', 'touchit')
    self.assertFalse(os.path.exists(os.path.dirname(path)))
    osutils.Touch(path, makedirs=True)
    self.assertTrue(os.path.exists(path))
    self.assertEqual(os.path.getsize(path), 0)


class TempDirTests(cros_test_lib.TestCase):
  """Unittests of osutils.TempDir.

  Unlike other test classes in this file, TempDirTestCase isn't used as a base
  class, because that is the functionality under test.
  """
  PREFIX = 'chromite.test.osutils.TempDirTests'

  class HelperException(Exception):
    """Exception for tests to raise to test exception handling."""

  class HelperExceptionInner(Exception):
    """Exception for tests to raise to test exception handling."""

  def testBasicSuccessEmpty(self):
    """Test we create and cleanup an empty tempdir."""
    with osutils.TempDir(prefix=self.PREFIX) as td:
      tempdir = td
      # Show the temp directory exists and is empty.
      self.assertTrue(os.path.isdir(tempdir))
      self.assertEquals(os.listdir(tempdir), [])

    # Show the temp directory no longer exists.
    self.assertNotExists(tempdir)

  def testBasicSuccessNotEmpty(self):
    """Test we cleanup tempdir with stuff in it."""
    with osutils.TempDir(prefix=self.PREFIX) as td:
      tempdir = td
      # Show the temp directory exists and is empty.
      self.assertTrue(os.path.isdir(tempdir))
      self.assertEquals(os.listdir(tempdir), [])

      # Create an empty file.
      osutils.Touch(os.path.join(tempdir, 'foo.txt'))

      # Create nested sub directories.
      subdir = os.path.join(tempdir, 'foo', 'bar', 'taco')
      os.makedirs(subdir)
      osutils.Touch(os.path.join(subdir, 'sauce.txt'))

    # Show the temp directory no longer exists.
    self.assertNotExists(tempdir)

  def testErrorCleanup(self):
    """Test we cleanup, even if an exception is raised."""
    try:
      with osutils.TempDir(prefix=self.PREFIX) as td:
        tempdir = td
        raise TempDirTests.HelperException()
    except TempDirTests.HelperException:
      pass

    # Show the temp directory no longer exists.
    self.assertNotExists(tempdir)

  def testCleanupExceptionContextException(self):
    """Test an exception during cleanup if the context DID raise."""
    was_raised = False
    tempdir_obj = osutils.TempDir(prefix=self.PREFIX)

    with mock.patch.object(osutils, '_TempDirTearDown',
                           side_effect=TempDirTests.HelperException):
      try:
        with tempdir_obj as td:
          tempdir = td
          raise TempDirTests.HelperExceptionInner()
      except TempDirTests.HelperExceptionInner:
        was_raised = True

    # Show that the exception exited the context.
    self.assertTrue(was_raised)

    # Verify the tempdir object no longer contains a reference to the tempdir.
    self.assertIsNone(tempdir_obj.tempdir)

    # Cleanup the dir leaked by our mock exception.
    os.rmdir(tempdir)

  def testCleanupExceptionNoContextException(self):
    """Test an exception during cleanup if the context did NOT raise."""
    was_raised = False
    tempdir_obj = osutils.TempDir(prefix=self.PREFIX)

    with mock.patch.object(osutils, '_TempDirTearDown',
                           side_effect=TempDirTests.HelperException):
      try:
        with tempdir_obj as td:
          tempdir = td
      except TempDirTests.HelperException:
        was_raised = True

    # Show that the exception exited the context.
    self.assertTrue(was_raised)

    # Verify the tempdir object no longer contains a reference to the tempdir.
    self.assertIsNone(tempdir_obj.tempdir)

    # Cleanup the dir leaked by our mock exception.
    os.rmdir(tempdir)


class MountTests(cros_test_lib.TestCase):
  """Unittests for osutils mounting and umounting helpers."""

  def testMountTmpfsDir(self):
    """Verify mounting a tmpfs works"""
    cleaned = False
    with osutils.TempDir(prefix='chromite.test.osutils') as tempdir:
      st_before = os.stat(tempdir)
      try:
        # Mount the dir and verify it worked.
        osutils.MountTmpfsDir(tempdir)
        st_after = os.stat(tempdir)
        self.assertNotEqual(st_before.st_dev, st_after.st_dev)

        # Unmount the dir and verify it worked.
        osutils.UmountDir(tempdir)
        cleaned = True

        # Finally make sure it's cleaned up.
        self.assertFalse(os.path.exists(tempdir))
      finally:
        if not cleaned:
          cros_build_lib.SudoRunCommand(['umount', '-lf', tempdir],
                                        error_code_ok=True)


class IteratePathParentsTest(cros_test_lib.TestCase):
  """Test parent directory iteration functionality."""

  def _RunForPath(self, path, expected):
    result_components = []
    for p in osutils.IteratePathParents(path):
      result_components.append(os.path.basename(p))

    result_components.reverse()
    if expected is not None:
      self.assertEquals(expected, result_components)

  def testIt(self):
    """Run the test vectors."""
    vectors = {
        '/': [''],
        '/path/to/nowhere': ['', 'path', 'to', 'nowhere'],
        '/path/./to': ['', 'path', 'to'],
        '//path/to': ['', 'path', 'to'],
        'path/to': None,
        '': None,
    }
    for p, e in vectors.iteritems():
      self._RunForPath(p, e)


class FindInPathParentsTest(cros_test_lib.TempDirTestCase):
  """Test FindInPathParents functionality."""

  D = cros_test_lib.Directory

  DIR_STRUCT = [
      D('a', [
          D('.repo', []),
          D('b', [
              D('c', [])
          ])
      ])
  ]

  START_PATH = os.path.join('a', 'b', 'c')

  def setUp(self):
    cros_test_lib.CreateOnDiskHierarchy(self.tempdir, self.DIR_STRUCT)

  def testFound(self):
    """Target is found."""
    found = osutils.FindInPathParents(
        '.repo', os.path.join(self.tempdir, self.START_PATH))
    self.assertEquals(found, os.path.join(self.tempdir, 'a', '.repo'))

  def testNotFound(self):
    """Target is not found."""
    found = osutils.FindInPathParents(
        'does.not/exist', os.path.join(self.tempdir, self.START_PATH))
    self.assertEquals(found, None)


class SourceEnvironmentTest(cros_test_lib.TempDirTestCase):
  """Test ostuil's environmental variable related methods."""

  ENV_WHITELIST = {
      'ENV1': 'monkeys like bananas',
      'ENV3': 'merci',
      'ENV6': '',
  }

  ENV_OTHER = {
      'ENV2': 'bananas are yellow',
      'ENV4': 'de rien',
  }

  ENV = """
declare -x ENV1="monkeys like bananas"
declare -x ENV2="bananas are yellow"
declare -x ENV3="merci"
declare -x ENV4="de rien"
declare -x ENV6=''
declare -x ENVA=('a b c' 'd' 'e 1234 %')
"""

  def setUp(self):
    self.env_file = os.path.join(self.tempdir, 'environment')
    osutils.WriteFile(self.env_file, self.ENV)

  def testWhiteList(self):
    env_dict = osutils.SourceEnvironment(
        self.env_file, ('ENV1', 'ENV3', 'ENV5', 'ENV6'))
    self.assertEquals(env_dict, self.ENV_WHITELIST)

  def testArrays(self):
    env_dict = osutils.SourceEnvironment(self.env_file, ('ENVA',))
    self.assertEquals(env_dict, {'ENVA': 'a b c,d,e 1234 %'})

    env_dict = osutils.SourceEnvironment(self.env_file, ('ENVA',), ifs=' ')
    self.assertEquals(env_dict, {'ENVA': 'a b c d e 1234 %'})


class DeviceInfoTests(cros_build_lib_unittest.RunCommandTestCase):
  """Tests methods retrieving information about devices."""

  FULL_OUTPUT = """
NAME="sda" RM="0" TYPE="disk" SIZE="128G"
NAME="sda1" RM="1" TYPE="part" SIZE="100G"
NAME="sda2" RM="1" TYPE="part" SIZE="28G"
NAME="sdc" RM="1" TYPE="disk" SIZE="7.4G"
NAME="sdc1" RM="1" TYPE="part" SIZE="1G"
NAME="sdc2" RM="1" TYPE="part" SIZE="6.4G"
"""

  PARTIAL_OUTPUT = """
NAME="sdc" RM="1" TYPE="disk" SIZE="7.4G"
NAME="sdc1" RM="1" TYPE="part" SIZE="1G"
NAME="sdc2" RM="1" TYPE="part" SIZE="6.4G"
"""

  def testListBlockDevices(self):
    """Tests that we can list all block devices correctly."""
    self.rc.AddCmdResult(partial_mock.Ignore(), output=self.FULL_OUTPUT)
    devices = osutils.ListBlockDevices()
    self.assertEqual(devices[0].NAME, 'sda')
    self.assertEqual(devices[0].RM, '0')
    self.assertEqual(devices[0].TYPE, 'disk')
    self.assertEqual(devices[0].SIZE, '128G')
    self.assertEqual(devices[3].NAME, 'sdc')
    self.assertEqual(devices[3].RM, '1')
    self.assertEqual(devices[3].TYPE, 'disk')
    self.assertEqual(devices[3].SIZE, '7.4G')

  def testGetDeviceSize(self):
    """Tests that we can get the size of a device."""
    self.rc.AddCmdResult(partial_mock.Ignore(), output=self.PARTIAL_OUTPUT)
    self.assertEqual(osutils.GetDeviceSize('/dev/sdc'), '7.4G')


class MountImagePartitionTests(cros_test_lib.MockTestCase):
  """Tests for MountImagePartition."""

  def setUp(self):
    self._gpt_table = {
        3: cros_build_lib.PartitionInfo(3, 1, 3, 2, 'fs', 'Label', 'flag')
    }

  def testWithCacheOkay(self):
    mount_dir = self.PatchObject(osutils, 'MountDir')
    osutils.MountImagePartition('image_file', 3, 'destination',
                                self._gpt_table)
    opts = ['loop', 'offset=1', 'sizelimit=2', 'ro']
    mount_dir.assert_called_with('image_file', 'destination', makedirs=True,
                                 skip_mtab=False, sudo=True, mount_opts=opts)

  def testWithCacheFail(self):
    self.assertRaises(ValueError, osutils.MountImagePartition,
                      'image_file', 404, 'destination', self._gpt_table)

  def testWithoutCache(self):
    self.PatchObject(cros_build_lib, 'GetImageDiskPartitionInfo',
                     return_value=self._gpt_table)
    mount_dir = self.PatchObject(osutils, 'MountDir')
    osutils.MountImagePartition('image_file', 3, 'destination')
    opts = ['loop', 'offset=1', 'sizelimit=2', 'ro']
    mount_dir.assert_called_with(
        'image_file', 'destination', makedirs=True, skip_mtab=False,
        sudo=True, mount_opts=opts
    )


class ChdirTests(cros_test_lib.MockTempDirTestCase):
  """Tests for ChdirContext."""

  def testChdir(self):
    current_dir = os.getcwd()
    self.assertNotEqual(self.tempdir, os.getcwd())
    with osutils.ChdirContext(self.tempdir):
      self.assertEqual(self.tempdir, os.getcwd())
    self.assertEqual(current_dir, os.getcwd())


class MountImageTests(cros_test_lib.MockTempDirTestCase):
  """Tests for MountImageContext."""

  def _testWithParts(self, parts, selectors, check_links=True):
    self.PatchObject(cros_build_lib, 'GetImageDiskPartitionInfo',
                     return_value=parts)
    mount_dir = self.PatchObject(osutils, 'MountDir')
    unmount_dir = self.PatchObject(osutils, 'UmountDir')
    rmdir = self.PatchObject(osutils, 'RmDir')
    with osutils.MountImageContext('_ignored', self.tempdir, selectors):
      for _, part in parts.items():
        mount_point = os.path.join(self.tempdir, 'dir-%d' % part.number)
        mount_dir.assert_any_call(
            '_ignored', mount_point, makedirs=True, skip_mtab=False,
            sudo=True,
            mount_opts=['loop', 'offset=0', 'sizelimit=0', 'ro']
        )
        if check_links:
          link = os.path.join(self.tempdir, 'dir-%s' % part.name)
          self.assertTrue(os.path.islink(link))
          self.assertEqual(os.path.basename(mount_point),
                           os.readlink(link))
    for _, part in parts.items():
      mount_point = os.path.join(self.tempdir, 'dir-%d' % part.number)
      unmount_dir.assert_any_call(mount_point, cleanup=False)
      rmdir.assert_any_call(mount_point, sudo=True)
      if check_links:
        link = os.path.join(self.tempdir, 'dir-%s' % part.name)
        self.assertFalse(os.path.lexists(link))

  def testWithPartitionNumber(self):
    parts = {
        1: cros_build_lib.PartitionInfo(1, 0, 0, 0, '', 'my-stateful', ''),
        3: cros_build_lib.PartitionInfo(3, 0, 0, 0, '', 'my-root-a', ''),
    }
    self._testWithParts(parts, [1, 3])

  def testWithPartitionLabel(self):
    parts = {
        42: cros_build_lib.PartitionInfo(42, 0, 0, 0, '', 'label', ''),
    }
    self._testWithParts(parts, ['label'])

  def testInvalidPartSelector(self):
    parts = {
        42: cros_build_lib.PartitionInfo(42, 0, 0, 0, '', 'label', ''),
    }
    self.assertRaises(ValueError, self._testWithParts, parts, ['label404'])
    self.assertRaises(ValueError, self._testWithParts, parts, [404])

  def testFailOnExistingMount(self):
    parts = {
        42: cros_build_lib.PartitionInfo(42, 0, 0, 0, '', 'label', ''),
    }
    os.makedirs(os.path.join(self.tempdir, 'dir-42'))
    self.assertRaises(ValueError, self._testWithParts, parts, [42])

  def testExistingLinkNotCleanedUp(self):
    parts = {
        42: cros_build_lib.PartitionInfo(42, 0, 0, 0, '', 'label', ''),
    }
    symlink = os.path.join(self.tempdir, 'dir-label')
    os.symlink('/tmp', symlink)
    self.assertEqual('/tmp', os.readlink(symlink))
    self._testWithParts(parts, [42], check_links=False)
    self.assertEqual('/tmp', os.readlink(symlink))


class IterateMountPointsTests(cros_test_lib.TempDirTestCase):
  """Test for IterateMountPoints function."""

  def setUp(self):
    self.proc_mount = os.path.join(self.tempdir, 'mounts')
    osutils.WriteFile(
        self.proc_mount,
        r'''/dev/loop0 /mnt/dir_8 ext4 rw,relatime,data=ordered 0 0
/dev/loop2 /mnt/dir_1 ext4 rw,relatime,data=ordered 0 0
/dev/loop1 /mnt/dir_12 vfat rw 0 0
/dev/loop4 /mnt/dir_3 ext4 ro,relatime 0 0
weird\040system /mnt/weirdo unknown ro 0 0
tmpfs /mnt/spaced\040dir tmpfs ro 0 0
tmpfs /mnt/\134 tmpfs ro 0 0
'''
    )

  def testOkay(self):
    r = list(osutils.IterateMountPoints(self.proc_mount))
    self.assertEqual(len(r), 7)
    self.assertEqual(r[0].source, '/dev/loop0')
    self.assertEqual(r[1].destination, '/mnt/dir_1')
    self.assertEqual(r[2].filesystem, 'vfat')
    self.assertEqual(r[3].options, 'ro,relatime')

  def testEscape(self):
    r = list(osutils.IterateMountPoints(self.proc_mount))
    self.assertEqual(r[4].source, 'weird system')
    self.assertEqual(r[5].destination, '/mnt/spaced dir')
    self.assertEqual(r[6].destination, '/mnt/\\')


class ResolveSymlinkTest(cros_test_lib.TestCase):
  """Tests for ResolveSymlink."""

  def testRelativeLink(self):
    os.symlink('target', 'link')
    self.assertEqual(osutils.ResolveSymlink('link'), 'target')
    os.unlink('link')

  def testAbsoluteLink(self):
    os.symlink('/target', 'link')
    self.assertEqual(osutils.ResolveSymlink('link'), '/target')
    self.assertEqual(osutils.ResolveSymlink('link', '/root'), '/root/target')
    os.unlink('link')

  def testRecursion(self):
    os.symlink('target', 'link1')
    os.symlink('link1', 'link2')
    self.assertEqual(osutils.ResolveSymlink('link2'), 'target')
    os.unlink('link2')
    os.unlink('link1')

  def testRecursionWithAbsoluteLink(self):
    os.symlink('target', 'link1')
    os.symlink('/link1', 'link2')
    self.assertEqual(osutils.ResolveSymlink('link2', '.'), './target')
    os.unlink('link2')
    os.unlink('link1')
