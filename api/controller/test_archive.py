# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test archive creation service.

The CI system requires archives of autotest files for a number of stages.
This service handles the creation of those archives.
"""

from __future__ import print_function

from chromite.lib import cros_build_lib
from chromite.service import test_archive


def CreateHwTestArchives(input_proto, output_proto):
  """Create the HW Test archives.

  Args:
    input_proto (CreateArchiveRequest): The input arguments message.
    output_proto (CreateArchiveResponse): The empty output message.
  """
  board = input_proto.build_target.name
  output_dir = input_proto.output_directory

  if not board:
    cros_build_lib.Die('A build target name is required.')
  if not output_dir:
    cros_build_lib.Die('An output directory must be specified.')

  try:
    files = test_archive.CreateHwTestArchives(board, output_dir)
  except test_archive.ArchiveBaseDirNotFound as e:
    cros_build_lib.Die(e.message)

  for current in files.values():
    new_file = output_proto.files.add()
    new_file.path = current
