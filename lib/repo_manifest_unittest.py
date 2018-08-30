# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for chromite.lib.repo_manifest."""

from __future__ import print_function

import io
import os
import pickle

from xml.etree import cElementTree as ElementTree

from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import repo_manifest


MANIFEST_OUTER_XML = """<?xml version="1.0" encoding="UTF-8"?>
<manifest>%s</manifest>
"""

REMOTES_XML = """
  <remote name="simple_remote"
          fetch="http://simple.example.com"/>
  <remote name="complex_remote" alias="cplx"
          fetch="http://example.com"
          pushurl="http://example.com/push"
          review="http://review.example.com"
          revision="refs/heads/master"/>
"""

DEFAULT_XML = '<default remote="simple_remote" upstream="default_upstream"/>'

SIMPLE_PROJECT_XML = '<project name="simple/project"/>'

COMPLEX_PROJECT_XML = """
  <project name="complex/project" path="src/complex" revision="cafe"
           remote="complex_remote" upstream="refs/heads/master">
    <annotation name="branch-mode" value="pin"/>
  </project>
"""

MANIFEST_XML = MANIFEST_OUTER_XML % (
    REMOTES_XML +
    DEFAULT_XML +
    SIMPLE_PROJECT_XML +
    COMPLEX_PROJECT_XML)


def ManifestToString(manifest):
  """Return the given Manifest's XML data as a string."""
  buf = io.BytesIO()
  manifest.Write(buf)
  return buf.getvalue()


class XMLTestCase(cros_test_lib.TestCase):
  """Mixin for XML tests."""

  def AssertXMLAlmostEqual(self, xml1, xml2):
    """Check that two XML strings are semanitcally equal."""
    def Normalize(xml):
      elem = ElementTree.fromstring(xml)
      return ElementTree.tostring(elem)
    self.assertMultiLineEqual(Normalize(xml1), Normalize(xml2))

  def ETreeFromString(self, xml_data):
    """Return an ElementTree object with the given data."""
    return ElementTree.ElementTree(ElementTree.fromstring(xml_data))


class ManifestTest(cros_test_lib.TempDirTestCase, XMLTestCase):
  """Tests for repo_manifest.Manifest."""

  def setUp(self):
    self.manifest = repo_manifest.Manifest.FromString(MANIFEST_XML)

  def testInitEmpty(self):
    """Test Manifest.__init__ on the emptiest valid input."""
    etree = self.ETreeFromString(MANIFEST_OUTER_XML % '')
    repo_manifest.Manifest(etree)

  def testInitInvalidManifest(self):
    """Test Manifest.__init__ on invalid input."""
    etree = self.ETreeFromString('<foo/>')
    with self.assertRaises(repo_manifest.InvalidManifest):
      repo_manifest.Manifest(etree)

  def testInitUnsupportedFeatures(self):
    """Test Manifest.__init__ on input with unsupported features."""
    for inner_xml in ('<project><project/></project>',
                      '<extend-project/>',
                      '<remove-project/>',
                      '<include/>'):
      etree = self.ETreeFromString(MANIFEST_OUTER_XML % inner_xml)
      with self.assertRaises(repo_manifest.UnsupportedFeature):
        repo_manifest.Manifest(etree)

  def testPickle(self):
    """Test Manifest picklability."""
    pickled = pickle.dumps(self.manifest)
    unpickled = pickle.loads(pickled)
    self.AssertXMLAlmostEqual(ManifestToString(unpickled), MANIFEST_XML)

  def testFromFile(self):
    """Test Manifest.FromFile."""
    path = os.path.join(self.tempdir, 'manifest.xml')
    osutils.WriteFile(path, MANIFEST_XML)
    manifest = repo_manifest.Manifest.FromFile(path)
    self.assertIsNotNone(manifest.GetUniqueProject('simple/project'))

  def testFromString(self):
    """Test Manifest.FromString."""
    manifest = repo_manifest.Manifest.FromString(MANIFEST_XML)
    self.assertIsNotNone(manifest.GetUniqueProject('simple/project'))

  def testWrite(self):
    """Test Manifest.Write."""
    manifest_data = ManifestToString(self.manifest)
    self.AssertXMLAlmostEqual(MANIFEST_XML, manifest_data)

  def testDefault(self):
    """Test Manifest.Default."""
    self.assertEqual(self.manifest.Default().remote, 'simple_remote')

  def testDefaultMissing(self):
    """Test Manifest.Default with no <default>."""
    manifest = repo_manifest.Manifest.FromString(MANIFEST_OUTER_XML % '')
    self.assertIsNone(manifest.Default().remote)

  def testRemotes(self):
    """Test Manifest.Remotes."""
    remote_names = [x.name for x in self.manifest.Remotes()]
    self.assertItemsEqual(remote_names, ['simple_remote', 'complex_remote'])

  def testGetRemote(self):
    """Test Manifest.GetRemote."""
    remote = self.manifest.GetRemote('simple_remote')
    self.assertEqual(remote.name, 'simple_remote')

  def testGetRemoteMissing(self):
    """Test Manifest.GetRemote without named <remote>."""
    with self.assertRaises(ValueError):
      self.manifest.GetRemote('missing')

  def testProjects(self):
    """Test Manifest.Projects."""
    project_names = [x.name for x in self.manifest.Projects()]
    self.assertItemsEqual(project_names, ['simple/project', 'complex/project'])

  def testGetUniqueProject(self):
    """Test Manifest.GetUniqueProject."""
    project = self.manifest.GetUniqueProject('simple/project')
    self.assertEqual(project.name, 'simple/project')

  def testGetUniqueProjectMissing(self):
    """Test Manifest.GetUniqueProject without named <project>."""
    with self.assertRaises(ValueError):
      self.manifest.GetUniqueProject('missing/project')


class ManifestElementExample(repo_manifest._ManifestElement):
  """_ManifestElement testing example."""
  # pylint: disable=protected-access

  ATTRS = ('name', 'other_attr')
  TAG = 'example'


class ManifestElementTest(XMLTestCase):
  """Tests for repo_manifest._ManifestElement."""
  # pylint: disable=attribute-defined-outside-init

  XML = '<example name="value"/>'

  def setUp(self):
    element = ElementTree.fromstring(self.XML)
    self.example = ManifestElementExample(None, element)

  def testPickle(self):
    """Test _ManifestElement picklability."""
    pickled = pickle.dumps(self.example)
    unpickled = pickle.loads(pickled)
    self.AssertXMLAlmostEqual(repr(unpickled), self.XML)

  def testGetters(self):
    """Test _ManifestElement.__getattr__."""
    self.assertEqual(self.example.name, 'value')
    self.assertIsNone(self.example.other_attr)

  def testGettersInvalidAttr(self):
    """Test _ManifestElement.__getattr__ with invalid attr."""
    with self.assertRaises(AttributeError):
      _ = self.example.invalid

  def testSetters(self):
    """Test _ManifestElement.__setattr__."""
    self.example.name = 'new'
    self.example.other_attr = 'other'
    EXPECTED = '<example name="new" other-attr="other"/>'
    self.AssertXMLAlmostEqual(repr(self.example), EXPECTED)


class RemoteTest(cros_test_lib.TestCase):
  """Tests for repo_manifest.Remote."""

  def setUp(self):
    self.manifest = repo_manifest.Manifest.FromString(MANIFEST_XML)
    self.simple = self.manifest.GetRemote('simple_remote')
    self.complex = self.manifest.GetRemote('complex_remote')

  def testGitName(self):
    self.assertEqual(self.simple.GitName(), 'simple_remote')

  def testGitNameAlias(self):
    self.assertEqual(self.complex.GitName(), 'cplx')

  def testPushURL(self):
    self.assertEqual(self.simple.PushURL(), 'http://simple.example.com')
    self.assertEqual(self.complex.PushURL(), 'http://example.com/push')


class ProjectTest(cros_test_lib.TestCase):
  """Tests for repo_manifest.Project."""

  def setUp(self):
    self.manifest = repo_manifest.Manifest.FromString(MANIFEST_XML)
    self.simple = self.manifest.GetUniqueProject('simple/project')
    self.complex = self.manifest.GetUniqueProject('complex/project')

  def testPath(self):
    """Test Project.Path."""
    self.assertEqual(self.simple.Path(), 'simple/project')
    self.assertEqual(self.complex.Path(), 'src/complex')

  def testRemoteName(self):
    """Test Project.RemoteName."""
    self.assertEqual(self.simple.RemoteName(), 'simple_remote')
    self.assertEqual(self.complex.RemoteName(), 'complex_remote')

  def testRemote(self):
    """Test Project.Remote."""
    self.assertEqual(self.simple.Remote().name, 'simple_remote')

  def testRevision(self):
    """Test Project.Revision."""
    self.assertIsNone(self.simple.Revision())
    self.assertEqual(self.complex.Revision(), 'cafe')
