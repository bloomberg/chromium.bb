# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for cq_config."""

from __future__ import print_function

import ConfigParser
import mock
import os
import osutils

from chromite.lib import cq_config
from chromite.lib import git
from chromite.lib import patch as cros_patch
from chromite.lib import patch_unittest


class GetOptionsTest(patch_unittest.MockPatchBase):
  """Tests for functions that get options from config file."""

  def GetOption(self, path, section='a', option='b'):
    # pylint: disable=protected-access
    return cq_config.CQConfigParser._GetOptionFromConfigFile(
        path, section, option)

  def testBadConfigFile(self):
    """Test if we can handle an incorrectly formatted config file."""
    with osutils.TempDir(set_global=True) as tempdir:
      path = os.path.join(tempdir, 'foo.ini')
      osutils.WriteFile(path, 'foobar')
      self.assertRaises(ConfigParser.Error, self.GetOption, path)

  def testMissingConfigFile(self):
    """Test if we can handle a missing config file."""
    with osutils.TempDir(set_global=True) as tempdir:
      path = os.path.join(tempdir, 'foo.ini')
      self.assertEqual(None, self.GetOption(path))

  def testGoodConfigFile(self):
    """Test if we can handle a good config file."""
    with osutils.TempDir(set_global=True) as tempdir:
      path = os.path.join(tempdir, 'foo.ini')
      osutils.WriteFile(path, '[a]\nb: bar baz\n')
      ignored = self.GetOption(path)
      self.assertEqual('bar baz', ignored)

  def testGetPreCQConfigs(self):
    """Test GetPreCQConfigs."""
    with osutils.TempDir(set_global=True) as tempdir:
      path = os.path.join(tempdir, 'foo.ini')
      osutils.WriteFile(path,
                        '[GENERAL]\npre-cq-configs: default binhost-pre-cq\n')
      self.PatchObject(cq_config.CQConfigParser, 'GetCommonConfigFileForChange',
                       return_value=path)
      cq_config_parser = cq_config.CQConfigParser(mock.Mock(), mock.Mock())
      pre_cq_configs = cq_config_parser.GetPreCQConfigs()
      self.assertItemsEqual(pre_cq_configs, ['default', 'binhost-pre-cq'])

  def testGetIgnoredStages(self):
    """Test if we can get the ignored stages from a good config file."""
    with osutils.TempDir(set_global=True) as tempdir:
      path = os.path.join(tempdir, 'foo.ini')
      osutils.WriteFile(path, '[GENERAL]\nignored-stages: bar baz\n')
      self.PatchObject(cq_config.CQConfigParser, 'GetCommonConfigFileForChange',
                       return_value=path)
      cq_config_parser = cq_config.CQConfigParser(mock.Mock(), mock.Mock())
      ignored = cq_config_parser.GetStagesToIgnore()
      self.assertEqual(ignored, ['bar', 'baz'])

  def testGetSubsystem(self):
    """Test if we can get the subsystem label from a good config file."""
    with osutils.TempDir(set_global=True) as tempdir:
      path = os.path.join(tempdir, 'foo.ini')
      osutils.WriteFile(path, '[GENERAL]\nsubsystem: power light\n')
      self.PatchObject(cq_config.CQConfigParser, 'GetCommonConfigFileForChange',
                       return_value=path)
      cq_config_parser = cq_config.CQConfigParser(mock.Mock(), mock.Mock())
      subsystems = cq_config_parser.GetSubsystems()
      self.assertItemsEqual(subsystems, ['power', 'light'])

  def testResultForBadConfigFileWithTrueForgiven(self):
    """Test whether the return is None when handle a malformat config file."""
    build_root = 'foo/build/root'
    change = self.GetPatches(how_many=1)
    with osutils.TempDir(set_global=True) as tempdir:
      path = os.path.join(tempdir, 'COMMIT-QUEUE.ini')
      osutils.WriteFile(path, 'foo\n')
      self.PatchObject(cq_config.CQConfigParser, 'GetCommonConfigFileForChange',
                       return_value=path)
      cq_config_parser = cq_config.CQConfigParser(build_root, change)
      result = cq_config_parser.GetOption('a', 'b')
      self.assertEqual(None, result)

  def testResultForBadConfigFileWithFalseForgiven(self):
    """Test whether exception is raised when handle a malformat config file."""
    build_root = 'foo/build/root'
    change = self.GetPatches(how_many=1)
    with osutils.TempDir(set_global=True) as tempdir:
      path = os.path.join(tempdir, 'COMMIT-QUEUE.ini')
      osutils.WriteFile(path, 'foo\n')
      self.PatchObject(cq_config.CQConfigParser, 'GetCommonConfigFileForChange',
                       return_value=path)
      cq_config_parser = cq_config.CQConfigParser(build_root, change)
      self.assertRaises(cq_config.MalformedCQConfigException,
                        cq_config_parser.GetOption, 'a', 'b', forgiven=False)


class ConfigFileTest(patch_unittest.MockPatchBase):
  """Tests for functions that read config information for a patch."""
  # pylint: disable=protected-access

  def _GetPatch(self, affected_files):
    return self.MockPatch(
        mock_diff_status={path: 'M' for path in affected_files})

  def testAffectedSubdir(self):
    """Test AffectedSubdir."""
    p = self._GetPatch(['a', 'b', 'c'])
    self.assertEqual(
        cq_config.CQConfigParser._GetCommonAffectedSubdir(p, '/a/b'), '/a/b')

    p = self._GetPatch(['a/a', 'a/b', 'a/c'])
    self.assertEqual(
        cq_config.CQConfigParser._GetCommonAffectedSubdir(p, '/a/b'), '/a/b/a')

    p = self._GetPatch(['a/a', 'a/b', 'a/c'])
    self.assertEqual(
        cq_config.CQConfigParser._GetCommonAffectedSubdir(p, '/a/b'), '/a/b/a')

  def testGetCommonConfigFileForChange(self):
    """Test GetCommonConfigFileForChange."""
    self.PatchObject(git.ManifestCheckout, 'Cached')
    p = self._GetPatch(['a/a', 'a/b', 'a/c'])
    mock_checkout = mock.Mock()
    self.PatchObject(cros_patch.GerritPatch, 'GetCheckout',
                     return_value=mock_checkout)

    self.PatchObject(os.path, 'isfile', return_value=True)
    mock_checkout.GetPath.return_value = '/a/b'
    self.assertEqual(
        cq_config.CQConfigParser.GetCommonConfigFileForChange('/', p),
        '/a/b/a/COMMIT-QUEUE.ini')
    mock_checkout.GetPath.return_value = '/a/b/'
    self.assertEqual(
        cq_config.CQConfigParser.GetCommonConfigFileForChange('/', p),
        '/a/b/a/COMMIT-QUEUE.ini')

    self.PatchObject(os.path, 'isfile', return_value=False)
    mock_checkout.GetPath.return_value = '/a/b'
    self.assertEqual(
        cq_config.CQConfigParser.GetCommonConfigFileForChange('/', p),
        '/a/b/COMMIT-QUEUE.ini')
    mock_checkout.GetPath.return_value = '/a/b/'
    self.assertEqual(
        cq_config.CQConfigParser.GetCommonConfigFileForChange('/', p),
        '/a/b/COMMIT-QUEUE.ini')
