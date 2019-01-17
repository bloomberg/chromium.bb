# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The Image API is the entry point for image functionality."""

from __future__ import print_function

import os

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import image_lib
from chromite.lib import path_util


class Error(Exception):
  """Base module error."""

class InvalidArgumentError(Exception):
  """Invalid argument values."""


def Test(board, result_directory, image_dir=None):
  """Run tests on an already built image.

  Currently this is just running test_image.

  Args:
    board (str): The board name.
    result_directory (str): Root directory where the results should be stored
      relative to the chroot.
    image_dir (str): The path to the image. Uses the board's default image
      build path when not provided.

  Returns:
    bool - True if all tests passed, False otherwise.
  """
  if not board:
    raise InvalidArgumentError('Board is required.')
  if not result_directory:
    raise InvalidArgumentError('Result directory required.')

  if not image_dir:
    # We can build the path to the latest image directory.
    image_dir = image_lib.GetLatestImageLink(board, force_chroot=True)
  elif not cros_build_lib.IsInsideChroot() and os.path.exists(image_dir):
    # Outside chroot with outside chroot path--we need to convert it.
    image_dir = path_util.ToChrootPath(image_dir)

  cmd = [
      os.path.join(constants.CHROOT_SOURCE_ROOT, constants.CHROMITE_BIN_SUBDIR,
                   'test_image'),
      '--board', board,
      '--test_results_root', result_directory,
      image_dir,
  ]

  result = cros_build_lib.SudoRunCommand(cmd, enter_chroot=True,
                                         error_code_ok=True)

  return result.returncode == 0
