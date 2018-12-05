# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Compile the Build API's proto."""

from __future__ import print_function

import os

from chromite.lib import commandline
from chromite.lib import cros_build_lib


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

  cmd = ('protoc --python_out %(output)s --proto_path %(source)s %(targets)s'
         % {'output': output, 'source': source, 'targets': targets})

  result = cros_build_lib.RunCommand(cmd, shell=True, error_code_ok=True)
  return result.returncode
