#!/usr/bin/python
#
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for the osutils.py module (imagine that!)."""

import os
import sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils


class TestOsutils(cros_test_lib.TempDirTestCase):

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


if __name__ == '__main__':
  cros_test_lib.main()
