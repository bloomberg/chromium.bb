# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Image service tests."""

from __future__ import print_function

import os

from chromite.api.gen import image_pb2
from chromite.api.service import image as image_service
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib.api import image as image_lib


class ImageTest(cros_test_lib.MockTempDirTestCase):
  """Image service tests."""

  def setUp(self):
    self.image_path = os.path.join(self.tempdir, 'image.bin')
    self.board = 'board'
    self.result_directory = os.path.join(self.tempdir, 'results')

    osutils.SafeMakedirs(self.result_directory)
    osutils.Touch(self.image_path)

  def testTestArgumentValidation(self):
    """Test function argument validation tests."""
    self.PatchObject(image_lib, 'Test', return_value=True)
    input_proto = image_pb2.TestImageRequest()
    output_proto = image_pb2.TestImageResult()

    # Nothing provided.
    with self.assertRaises(image_service.InvalidTestArgumentError):
      image_service.Test(input_proto, output_proto)

    # Just one argument.
    input_proto.build_target.name = self.board
    with self.assertRaises(image_service.InvalidTestArgumentError):
      image_service.Test(input_proto, output_proto)

    # Two arguments provided.
    input_proto.result.directory = self.result_directory
    with self.assertRaises(image_service.InvalidTestArgumentError):
      image_service.Test(input_proto, output_proto)

    # Invalid image path.
    input_proto.image.path = '/invalid/image/path'
    with self.assertRaises(image_service.InvalidTestArgumentError):
      image_service.Test(input_proto, output_proto)

    # All valid arguments.
    input_proto.image.path = self.image_path
    image_service.Test(input_proto, output_proto)

  def testTestOutputHandling(self):
    """Test function output tests."""
    input_proto = image_pb2.TestImageRequest()
    input_proto.image.path = self.image_path
    input_proto.build_target.name = self.board
    input_proto.result.directory = self.result_directory
    output_proto = image_pb2.TestImageResult()

    self.PatchObject(image_lib, 'Test', return_value=True)
    image_service.Test(input_proto, output_proto)
    self.assertTrue(output_proto.success)

    self.PatchObject(image_lib, 'Test', return_value=False)
    image_service.Test(input_proto, output_proto)
    self.assertFalse(output_proto.success)
