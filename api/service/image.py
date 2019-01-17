# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Image API Service.

The image related API endpoints should generally be found here.
"""

from __future__ import print_function

import os

from chromite.lib.api import image

class Error(Exception):
  """The module's base error class."""


class InvalidTestArgumentError(Error):
  """Invalid argument to the Test function."""


def Test(input_proto, output_proto):
  """Run image tests.

  Args:
    input_proto (image_pb2.ImageTestRequest): The input message.
    output_proto (image_pb2.ImageTestResult): The output message.
  """
  image_path = input_proto.image.path
  board = input_proto.build_target.name
  result_directory = input_proto.result.directory

  if not board:
    raise InvalidTestArgumentError('The build_target.name is required.')
  if not result_directory:
    raise InvalidTestArgumentError('The result.directory is required.')
  if not image_path:
    raise InvalidTestArgumentError('The image.path is required.')

  if not os.path.isfile(image_path) or not image_path.endswith('.bin'):
    raise InvalidTestArgumentError(
        'The image.path must be an existing image file with a .bin extension.')

  output_proto.success = image.Test(board, result_directory,
                                    image_dir=image_path)
