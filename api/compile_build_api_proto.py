# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Compile the Build API's proto."""

from __future__ import print_function

import os

from chromite.lib import commandline
from chromite.lib import constants
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

  base_dir = os.path.join(constants.CHROOT_SOURCE_ROOT, 'chromite', 'api')
  output = os.path.join(base_dir, 'gen')
  source = os.path.join(base_dir, 'proto')
  targets = os.path.join(source, '*.proto')

  version = cros_build_lib.RunCommand(['protoc', '--version'], print_cmd=False,
                                      enter_chroot=True, capture_output=True,
                                      error_code_ok=True)
  if version.returncode != 0:
    cros_build_lib.Die('protoc not found in your chroot.')
  elif '3.3.0' in version.output:
    # This is the old chroot version, just needs to have update_chroot run.
    cros_build_lib.Die('Old protoc version detected. Please update your chroot'
                       'and try again: `cros_sdk -- ./update_chroot`')
  elif '3.6.1' not in version.output:
    # Note: We know some lower versions have some compiling backwards
    # compatibility problems. One would hope new versions would be ok,
    # but we would have said that with earlier versions too.
    logging.warning('Unsupported protoc version found in your chroot.\n'
                    "libprotoc 3.6.1 is supported. Found '%s'.\n"
                    'protoc will still be run, but be cautious.',
                    version.output.strip())

  cmd = ('protoc --python_out %(output)s --proto_path %(source)s %(targets)s'
         % {'output': output, 'source': source, 'targets': targets})
  result = cros_build_lib.RunCommand(cmd, enter_chroot=True, shell=True,
                                     error_code_ok=True)
  return result.returncode
