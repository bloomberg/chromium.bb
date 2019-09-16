# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Compile the Build API's proto.

Install proto using CIPD to ensure a consistent protoc version.
"""

from __future__ import print_function

import os

from chromite.lib import commandline
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import osutils

_API_DIR = os.path.join(constants.CHROMITE_DIR, 'api')
_CIPD_ROOT = os.path.join(constants.CHROMITE_DIR, '.cipd_bin')
_PROTOC = os.path.join(_CIPD_ROOT, 'protoc')
_PROTO_DIR = os.path.join(constants.CHROMITE_DIR, 'infra', 'proto')

PROTOC_VERSION = '3.6.1'


class Error(Exception):
  """Base error class for the module."""


class GenerationError(Error):
  """A failure we can't recover from."""


def _InstallProtoc():
  """Install protoc from CIPD."""
  logging.info('Installing protoc.')
  cmd = ['cipd', 'ensure']
  # Clean up the output.
  cmd.extend(['-log-level', 'warning'])
  # Set the install location.
  cmd.extend(['-root', _CIPD_ROOT])

  ensure_content = ('infra/tools/protoc/${platform} '
                    'protobuf_version:v%s' % PROTOC_VERSION)
  with osutils.TempDir() as tempdir:
    ensure_file = os.path.join(tempdir, 'cipd_ensure_file')
    osutils.WriteFile(ensure_file, ensure_content)

    cmd.extend(['-ensure-file', ensure_file])

    cros_build_lib.RunCommand(cmd, cwd=constants.CHROMITE_DIR, print_cmd=False)


def _CleanTargetDirectory(directory):
  """Remove any existing generated files in the directory.

  This clean only removes the generated files to avoid accidentally destroying
  __init__.py customizations down the line. That will leave otherwise empty
  directories in place if things get moved. Neither case is relevant at the
  time of writing, but lingering empty directories seemed better than
  diagnosing accidental __init__.py changes.

  Args:
    directory (str): Path to be cleaned up.
  """
  logging.info('Cleaning old files.')
  for dirpath, _dirnames, filenames in os.walk(directory):
    old = [os.path.join(dirpath, f) for f in filenames if f.endswith('_pb2.py')]
    # Remove empty init files to clean up otherwise empty directories.
    if '__init__.py' in filenames:
      init = os.path.join(dirpath, '__init__.py')
      if not osutils.ReadFile(init):
        old.append(init)

    for current in old:
      osutils.SafeUnlink(current)


def _GenerateFiles(source, output):
  """Generate the proto files from the |source| tree into |output|.

  Args:
    source (str): Path to the proto source root directory.
    output (str): Path to the output root directory.
  """
  logging.info('Generating files.')
  targets = []

  # Only compile the subset we need for the API.
  subdirs = [
      os.path.join(source, 'chromite'),
      os.path.join(source, 'chromiumos'),
      os.path.join(source, 'test_platform')
  ]
  for basedir in subdirs:
    for dirpath, _dirnames, filenames in os.walk(basedir):
      for filename in filenames:
        if filename.endswith('.proto'):
          # We have a match, add the file.
          targets.append(os.path.join(dirpath, filename))

  cmd = [_PROTOC, '--python_out', output, '--proto_path', source] + targets
  result = cros_build_lib.RunCommand(
      cmd, cwd=source, print_cmd=False, error_code_ok=True)

  if result.returncode:
    raise GenerationError('Error compiling the proto. See the output for a '
                          'message.')


def _InstallMissingInits(directory):
  """Add any __init__.py files not present in the generated protobuf folders."""
  logging.info('Adding missing __init__.py files.')
  for dirpath, _dirnames, filenames in os.walk(directory):
    if '__init__.py' not in filenames:
      osutils.Touch(os.path.join(dirpath, '__init__.py'))


def _PostprocessFiles(directory):
  """Do postprocessing on the generated files.

  Args:
    directory (str): The root directory containing the generated files that are
      to be processed.
  """
  logging.info('Postprocessing: Fix imports.')
  # We are using a negative address here (the /address/! portion of the sed
  # command) to make sure we don't change any imports from protobuf itself.
  address = '^from google.protobuf'
  # Find: 'from x import y_pb2 as x_dot_y_pb2'.
  # "\(^google.protobuf[^ ]*\)" matches the module we're importing from.
  #   - \( and \) are for groups in sed.
  #   - ^google.protobuf prevents changing the import for protobuf's files.
  #   - [^ ] = Not a space. The [:space:] character set is too broad, but would
  #       technically work too.
  find = r'^from \([^ ]*\) import \([^ ]*\)_pb2 as \([^ ]*\)$'
  # Substitute: 'from chromite.api.gen.x import y_pb2 as x_dot_y_pb2'.
  sub = 'from chromite.api.gen.\\1 import \\2_pb2 as \\3'
  from_sed = [
      'sed', '-i',
      '/%(address)s/!s/%(find)s/%(sub)s/g' % {
          'address': address,
          'find': find,
          'sub': sub
      }
  ]

  for dirpath, _dirnames, filenames in os.walk(directory):
    # Update the
    pb2 = [os.path.join(dirpath, f) for f in filenames if f.endswith('_pb2.py')]
    if pb2:
      cmd = from_sed + pb2
      cros_build_lib.RunCommand(cmd, print_cmd=False)


def CompileProto(output=None):
  """Compile the Build API protobuf files.

  By default this will compile from infra/proto/src to api/gen. The output
  directory may be changed, but the imports will always be treated as if it is
  in the default location.

  Args:
    output (str|None): The output directory.
  """
  source = os.path.join(_PROTO_DIR, 'src')
  output = output or os.path.join(_API_DIR, 'gen')

  _InstallProtoc()
  _CleanTargetDirectory(output)
  _GenerateFiles(source, output)
  _InstallMissingInits(output)
  _PostprocessFiles(output)


def GetParser():
  """Build the argument parser."""
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument(
      '--destination',
      type='path',
      help='The directory where the proto should be generated. Defaults to '
           'the correct directory for the API.')
  return parser


def _ParseArguments(argv):
  """Parse and validate arguments."""
  parser = GetParser()
  opts = parser.parse_args(argv)

  opts.Freeze()
  return opts


def main(argv):
  opts = _ParseArguments(argv)

  try:
    CompileProto(output=opts.destination)
  except Error as e:
    logging.error(e.message)
    return 1
