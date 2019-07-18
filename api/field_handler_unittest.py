# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""field_handler module tests."""

from __future__ import print_function

import os

from chromite.api import field_handler
from chromite.api.gen.chromite.api import build_api_test_pb2
from chromite.api.gen.chromiumos import common_pb2
from chromite.lib import chroot_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils


class ChrootHandlerTest(cros_test_lib.TestCase):
  """ChrootHandler tests."""

  def setUp(self):
    self.path = '/chroot/dir'
    self.cache_dir = '/cache/dir'
    self.chrome_dir = '/chrome/dir'
    self.env = {'FEATURES': 'thing'}
    self.expected_chroot = chroot_lib.Chroot(path=self.path,
                                             cache_dir=self.cache_dir,
                                             chrome_root=self.chrome_dir,
                                             env=self.env)

  def test_parse_chroot_success(self):
    """Test successful Chroot message parse."""
    chroot_msg = common_pb2.Chroot()
    chroot_msg.path = self.path
    chroot_msg.cache_dir = self.cache_dir
    chroot_msg.chrome_dir = self.chrome_dir
    chroot_msg.env.features.add().feature = 'thing'

    chroot_handler = field_handler.ChrootHandler(clear_field=False)
    parsed_chroot = chroot_handler.parse_chroot(chroot_msg)

    self.assertEqual(self.expected_chroot, parsed_chroot)

  def test_handle_success(self):
    """Test a successful Chroot message parse from a parent message."""
    message = build_api_test_pb2.TestRequestMessage()
    message.chroot.path = self.path
    message.chroot.cache_dir = self.cache_dir
    message.chroot.chrome_dir = self.chrome_dir
    message.chroot.env.features.add().feature = 'thing'

    # First a no-clear parse.
    chroot_handler = field_handler.ChrootHandler(clear_field=False)
    chroot = chroot_handler.handle(message)

    self.assertEqual(self.expected_chroot, chroot)
    self.assertEqual(message.chroot.path, self.path)

    # A clear field parse.
    clear_chroot_handler = field_handler.ChrootHandler(clear_field=True)
    chroot = clear_chroot_handler.handle(message)

    self.assertEqual(self.expected_chroot, chroot)
    self.assertFalse(message.chroot.path)

  def test_handle_empty_chroot_message(self):
    """Test handling of an empty chroot message."""
    message = build_api_test_pb2.TestRequestMessage()
    empty_chroot = chroot_lib.Chroot(env={'FEATURES': 'separatedebug'})

    chroot_handler = field_handler.ChrootHandler(clear_field=False)
    chroot = chroot_handler.handle(message)

    self.assertEqual(empty_chroot, chroot)


class PathHandlerTest(cros_test_lib.TempDirTestCase):
  """PathHandler tests."""

  def setUp(self):
    self.source_dir = os.path.join(self.tempdir, 'source')
    self.dest_dir = os.path.join(self.tempdir, 'destination')
    osutils.SafeMakedirs(self.source_dir)
    osutils.SafeMakedirs(self.dest_dir)

    self.source_file1 = os.path.join(self.source_dir, 'file1')
    self.file1_contents = 'file 1'
    osutils.WriteFile(self.source_file1, self.file1_contents)

    self.file2_contents = 'some data'
    self.source_file2 = os.path.join(self.source_dir, 'file2')
    osutils.WriteFile(self.source_file2, self.file2_contents)

  def _path_checks(self, source_file, dest_file, contents=None):
    """Set of common checks for the copied files/directories."""
    # Message should now reflect the new path.
    self.assertNotEqual(source_file, dest_file)
    # The new path should be in the destination directory.
    self.assertStartsWith(dest_file, self.dest_dir)
    # The new file should exist.
    self.assertExists(dest_file)

    if contents:
      # The contents should be the same as the source file.
      self.assertFileContents(dest_file, contents)

  def test_handle_file(self):
    """Test handling of a single file."""
    message = build_api_test_pb2.TestRequestMessage()
    message.path.path = self.source_file1
    message.path.location = common_pb2.Path.OUTSIDE

    with field_handler.handle_paths(message, self.dest_dir, delete=True):
      new_path = message.path.path
      self._path_checks(self.source_file1, new_path, self.file1_contents)

    # The file should have been deleted on exit with delete=True.
    self.assertNotExists(new_path)
    # Make sure it gets reset.
    self.assertEqual(message.path.path, self.source_file1)

  def test_handle_files(self):
    """Test handling of multiple files."""
    message = build_api_test_pb2.TestRequestMessage()
    message.path.path = self.source_file1
    message.path.location = common_pb2.Path.OUTSIDE
    message.another_path.path = self.source_file2
    message.another_path.location = common_pb2.Path.OUTSIDE

    with field_handler.handle_paths(message, self.dest_dir, delete=False):
      new_path1 = message.path.path
      new_path2 = message.another_path.path

      self._path_checks(self.source_file1, new_path1, self.file1_contents)
      self._path_checks(self.source_file2, new_path2, self.file2_contents)

    # The files should still exist with delete=False.
    self.assertExists(new_path1)
    self.assertExists(new_path2)

  def test_handle_nested_file(self):
    """Test the nested path handling."""
    message = build_api_test_pb2.TestRequestMessage()
    message.nested_path.path.path = self.source_file1
    message.nested_path.path.location = common_pb2.Path.OUTSIDE

    with field_handler.handle_paths(message, self.dest_dir):
      new_path = message.nested_path.path.path
      self._path_checks(self.source_file1, new_path, self.file1_contents)

  def test_handle_directory(self):
    """Test handling of a directory."""
    message = build_api_test_pb2.TestRequestMessage()
    message.path.path = self.source_dir
    message.path.location = common_pb2.Path.OUTSIDE

    with field_handler.handle_paths(message, self.dest_dir):
      new_path = message.path.path

      self._path_checks(self.source_dir, self.dest_dir)
      # Make sure both directories have the same files.
      self.assertItemsEqual(os.listdir(self.source_dir), os.listdir(new_path))

  def test_direction(self):
    """Test the direction argument preventing copies."""
    message = build_api_test_pb2.TestRequestMessage()
    message.path.path = self.source_file1
    message.path.location = common_pb2.Path.INSIDE

    direction = field_handler.PathHandler.INSIDE
    with field_handler.handle_paths(message, self.dest_dir, delete=True,
                                    direction=direction):
      self.assertEqual(self.source_file1, message.path.path)

    # It should not be deleting the file when it doesn't need to copy it even
    # with delete=True.
    self.assertExists(self.source_file1)

  def test_prefix_inside(self):
    """Test the transfer inside prefix handling."""
    message = build_api_test_pb2.TestRequestMessage()
    message.path.path = self.source_dir
    message.path.location = common_pb2.Path.OUTSIDE

    with field_handler.handle_paths(message, self.dest_dir,
                                    direction=field_handler.PathHandler.INSIDE,
                                    prefix=self.tempdir):
      new_path = message.path.path
      # The prefix should be removed.
      self.assertFalse(new_path.startswith(self.tempdir))

  def test_prefix_outside(self):
    """Test the transfer outside prefix handling."""
    message = build_api_test_pb2.TestRequestMessage()
    message.path.path = self.source_dir.replace(self.tempdir, '')
    message.path.location = common_pb2.Path.INSIDE

    with field_handler.handle_paths(message, self.dest_dir,
                                    direction=field_handler.PathHandler.OUTSIDE,
                                    prefix=self.tempdir):
      new_path = message.path.path
      # The source directory should have been properly reconstructed and
      # everything copied over to the destination.
      self.assertItemsEqual(os.listdir(self.source_dir), os.listdir(new_path))


class HandleResultPathsTest(cros_test_lib.TempDirTestCase):
  """Tests for handle_result_paths."""

  def setUp(self):
    # Setup the directories.
    self.chroot_dir = os.path.join(self.tempdir, 'chroot')
    self.source_dir = '/source'
    self.chroot_source = os.path.join(self.chroot_dir,
                                      self.source_dir.lstrip(os.sep))
    self.source_dir2 = '/source2'
    self.chroot_source2 = os.path.join(self.chroot_dir,
                                       self.source_dir2.lstrip(os.sep))
    self.dest_dir = os.path.join(self.tempdir, 'destination')
    osutils.SafeMakedirs(self.chroot_source)
    osutils.SafeMakedirs(self.chroot_source2)
    osutils.SafeMakedirs(self.dest_dir)

    # Two files in the same directory inside the chroot.
    self.source_file1 = os.path.join(self.chroot_source, 'file1')
    self.source_file1_inside = os.path.join(self.source_dir, 'file1')
    self.file1_contents = 'file 1'
    osutils.WriteFile(self.source_file1, self.file1_contents)

    self.file2_contents = 'some data'
    self.source_file2 = os.path.join(self.chroot_source, 'file2')
    self.source_file2_inside = os.path.join(self.source_dir, 'file2')
    osutils.WriteFile(self.source_file2, self.file2_contents)

    # Third file in a different location.
    self.file3_contents = 'another file'
    self.source_file3 = os.path.join(self.chroot_source2, 'file3')
    self.source_file3_inside = os.path.join(self.source_dir2, 'file3')
    osutils.WriteFile(self.source_file3, self.file3_contents)

    self.request = build_api_test_pb2.TestRequestMessage()
    self.request.result_path.path.path = self.dest_dir
    self.request.result_path.path.location = common_pb2.Path.OUTSIDE
    self.response = build_api_test_pb2.TestResultMessage()
    self.chroot = chroot_lib.Chroot(path=self.chroot_dir)

  def _path_checks(self, path, destination, contents=None):
    self.assertTrue(path)
    self.assertStartsWith(path, destination)
    self.assertExists(path)
    if contents:
      self.assertFileContents(path, contents)

  def test_single_file(self):
    """Test a single file."""
    self.response.artifact.path = self.source_file1_inside
    self.response.artifact.location = common_pb2.Path.INSIDE

    field_handler.handle_result_paths(self.request, self.response, self.chroot)

    self._path_checks(self.response.artifact.path, self.dest_dir,
                      contents=self.file1_contents)

  def test_single_directory(self):
    """Test a single directory."""
    self.response.artifact.path = self.source_dir
    self.response.artifact.location = common_pb2.Path.INSIDE

    field_handler.handle_result_paths(self.request, self.response, self.chroot)

    self._path_checks(self.response.artifact.path, self.dest_dir)
    self.assertItemsEqual(os.listdir(self.chroot_source),
                          os.listdir(self.response.artifact.path))

  def test_multiple_files(self):
    """Test multiple files."""
    self.response.artifact.path = self.source_file1_inside
    self.response.artifact.location = common_pb2.Path.INSIDE
    self.response.nested_artifact.path.path = self.source_file2_inside
    self.response.nested_artifact.path.location = common_pb2.Path.INSIDE

    artifact3 = self.response.artifacts.add()
    artifact3.path = self.source_file3_inside
    artifact3.location = common_pb2.Path.INSIDE

    field_handler.handle_result_paths(self.request, self.response, self.chroot)

    self._path_checks(self.response.artifact.path, self.dest_dir,
                      contents=self.file1_contents)
    self._path_checks(self.response.nested_artifact.path.path, self.dest_dir,
                      contents=self.file2_contents)

    self.assertEqual(1, len(self.response.artifacts))
    for artifact in self.response.artifacts:
      self._path_checks(artifact.path, self.dest_dir,
                        contents=self.file3_contents)

  def test_multiple_directories(self):
    """Test multiple directories."""
    self.response.artifact.path = self.source_dir
    self.response.artifact.location = common_pb2.Path.INSIDE
    self.response.nested_artifact.path.path = self.source_dir2
    self.response.nested_artifact.path.location = common_pb2.Path.INSIDE

    field_handler.handle_result_paths(self.request, self.response, self.chroot)

    self._path_checks(self.response.artifact.path, self.dest_dir)
    self._path_checks(self.response.nested_artifact.path.path, self.dest_dir)

    expected = os.listdir(self.chroot_source)
    expected.extend(os.listdir(self.chroot_source2))
    self.assertItemsEqual(expected, os.listdir(self.response.artifact.path))
