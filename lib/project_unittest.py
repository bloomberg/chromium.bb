# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the project library."""

from __future__ import print_function

import mock
import os

from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import project


class ProjectTest(cros_test_lib.TempDirTestCase):
  """Unittest for project.py"""

  # pylint: disable=protected-access

  def setUp(self):
    self.project = None

  def CreateNewProject(self):
    """Creates a new project."""
    self.project = project.Project(self.tempdir, initial_config={'name': 'foo'})

  def SetupLegacyProject(self, project_dir=None, project_name='foo'):
    """Sets up a legacy project layout."""
    if project_dir is None:
      project_dir = self.tempdir
    layout_conf = 'repo-name = %s\n' % project_name
    osutils.WriteFile(os.path.join(project_dir, 'metadata', 'layout.conf'),
                      layout_conf, makedirs=True)

  def testLayoutFormat(self):
    """Test that layout.conf is correctly formatted."""
    self.CreateNewProject()
    content = {'repo-name': 'hello',
               'bar': 'foo'}
    self.project._WriteLayoutConf(content)

    layout_conf = osutils.ReadFile(os.path.join(self.tempdir, 'metadata',
                                                'layout.conf')).split('\n')

    expected_lines = ['repo-name = hello',
                      'bar = foo',
                      'profile-formats = portage-2']
    for line in expected_lines:
      self.assertTrue(line in layout_conf)

  def testWriteParents(self):
    """Test that the parent file is correctly formatted."""
    self.CreateNewProject()
    parents = ['hello:bonjour',
               'foo:bar']

    self.project._WriteParents(parents)
    parents_content = osutils.ReadFile(os.path.join(self.tempdir, 'profiles',
                                                    'base', 'parent'))

    self.assertEqual('hello:bonjour\nfoo:bar\n', parents_content)

  def testConfigurationGenerated(self):
    """Test that portage's files are generated when project.json changes."""
    self.CreateNewProject()
    sample_config = {'name': 'hello',
                     'dependencies': []}

    self.project.UpdateConfig(sample_config)

    self.assertExists(os.path.join(self.tempdir, 'profiles', 'base', 'parent'))
    self.assertExists(os.path.join(self.tempdir, 'metadata', 'layout.conf'))

  def testFindProjectInPath(self):
    """Test that we can infer the current project from the current directory."""
    self.CreateNewProject()
    os.remove(os.path.join(self.tempdir, 'project.json'))
    project_dir = os.path.join(self.tempdir, 'foo', 'bar', 'project')
    content = {'name': 'hello'}
    project.Project(project_dir, initial_config={'name': 'hello'})

    with osutils.ChdirContext(self.tempdir):
      self.assertEqual(None, project.FindProjectInPath())

    with osutils.ChdirContext(project_dir):
      self.assertEqual(content, project.FindProjectInPath().config)

    subdir = os.path.join(project_dir, 'sub', 'directory')
    osutils.SafeMakedirs(subdir)
    with osutils.ChdirContext(subdir):
      self.assertEqual(content, project.FindProjectInPath().config)

  def testProjectByNameExact(self):
    """Test that we can get the project for a given name."""
    first = os.path.join(self.tempdir, 'foo')
    osutils.WriteFile(os.path.join(first, 'make.conf'), 'hello', makedirs=True)

    second = os.path.join(self.tempdir, 'bar')
    project.Project(second, initial_config={'name': 'bar'})

    hello = os.path.join(self.tempdir, 'hello')
    project.Project(hello, initial_config={'name': 'hello'})

    hello_private = os.path.join(self.tempdir, 'hello-private')
    project.Project(hello_private, initial_config={'name': 'hello-private'})

    with mock.patch('chromite.lib.portage_util.FindOverlays',
                    return_value=[first, second, hello, hello_private]):
      self.assertEquals(hello, project.FindProjectByName('hello').project_dir)

  def testProjectByNamePrivate(self):
    """Test that we can get the project even if the overlay is private."""
    first = os.path.join(self.tempdir, 'foo')
    osutils.WriteFile(os.path.join(first, 'make.conf'), 'hello', makedirs=True)

    second = os.path.join(self.tempdir, 'bar')
    project.Project(second, initial_config={'name': 'bar'})

    hello_private = os.path.join(self.tempdir, 'hello-private')
    project.Project(hello_private, initial_config={'name': 'hello-private'})

    with mock.patch('chromite.lib.portage_util.FindOverlays',
                    return_value=[first, second, hello_private]):
      self.assertEquals(hello_private,
                        project.FindProjectByName('hello').project_dir)

  def testProjectCreation(self):
    """Test that project initialization throws the right errors."""
    self.CreateNewProject()
    with self.assertRaises(project.ProjectAlreadyExists):
      project.Project(self.tempdir, initial_config={})

    nonexistingproject = os.path.join(self.tempdir, 'foo')
    with self.assertRaises(project.ProjectNotFound):
      project.Project(nonexistingproject)

  def testLoadNonExistingProjectFails(self):
    """Tests that trying to load a non-existing project fails."""
    self.assertRaises(project.ProjectNotFound, project.Project, self.tempdir)

  def testLoadExistingNormalProjectSucceeds(self):
    """Tests that loading an existing project works."""
    self.CreateNewProject()
    self.project = project.Project(self.tempdir, allow_legacy=False)
    self.assertEquals('foo', self.project.config.get('name'))

  def testLoadExistingLegacyProjectFailsIfNotAllowed(self):
    """Tests that loading a legacy project fails when not allowed."""
    self.SetupLegacyProject()
    with self.assertRaises(project.ProjectNotFound):
      project.Project(self.tempdir, allow_legacy=False)

  def testLoadExistingLegacyProjectSucceeds(self):
    """Tests that loading a legacy project fails when not allowed."""
    self.SetupLegacyProject()
    self.project = project.Project(self.tempdir)
    self.assertEquals('foo', self.project.config.get('name'))

  def testLegacyProjectUpdateConfigFails(self):
    """Tests that a legacy project config cannot be updated."""
    self.SetupLegacyProject()
    self.project = project.Project(self.tempdir)
    with self.assertRaises(project.ProjectFeatureNotSupported):
      self.project.UpdateConfig({'name': 'bar'})

  def testInherits(self):
    """Tests the containment checking works as intended."""
    self.CreateNewProject()
    bar_overlay = os.path.join(self.tempdir, 'bar')
    self.SetupLegacyProject(project_dir=bar_overlay, project_name='bar')
    with mock.patch('chromite.lib.portage_util.FindOverlays',
                    return_value=[bar_overlay]):
      self.assertTrue(self.project.Inherits('bar'))
      self.assertFalse(self.project.Inherits('baz'))
