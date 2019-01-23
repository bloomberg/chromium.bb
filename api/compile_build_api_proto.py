# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Compile the Build API's proto."""

from __future__ import print_function

import os

from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging


def GetParser():
  parser = commandline.ArgumentParser(description=__doc__)
  return parser


def _ParseArguments(argv):
  """Parse and validate arguments."""
  parser = GetParser()
  opts = parser.parse_args(argv)

  opts.Freeze()
  return opts


def main(argv):
  _opts = _ParseArguments(argv)

  base_dir = os.path.abspath(os.path.join(__file__, '..'))
  output = os.path.join(base_dir, 'gen')
  source = os.path.join(base_dir, 'proto')
  targets = os.path.join(source, '*.proto')

  # TODO(crbug.com/924660) Update compile to run in the chroot and remove
  # the warning.
  protoc_version = ['protoc', '--version']
  result = cros_build_lib.RunCommand(protoc_version, print_cmd=False,
                                     redirect_stdout=True,
                                     combine_stdout_stderr=True,
                                     error_code_ok=True)
  if not result.returncode == 0 or not '3.6.1' in result.output:
    logging.warning('You must have libprotoc 3.6.1 installed locally to '
                    'compile the protobuf correctly.')
    logging.warning('This will be run in the chroot in the future '
                    '(see crbug.com/924660).')
    if not result.returncode == 0:
      logging.warning('protoc could not be found on your system.')
    else:
      logging.warning('"%s" was found on your system.', result.output.strip())

    logging.warning("We won't stop you from running it for now, but be very "
                    "weary of your changes.")

  cmd = ('protoc --python_out %(output)s --proto_path %(source)s %(targets)s'
         % {'output': output, 'source': source, 'targets': targets})

  result = cros_build_lib.RunCommand(cmd, shell=True, error_code_ok=True)
  return result.returncode
