# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Contains a helper function for deploying and executing a packaged
executable on a Target."""

import common
import json
import logging
import os
import shutil
import subprocess
import tempfile
import uuid

from symbolizer import FilterStream

FAR = os.path.join(common.SDK_ROOT, 'tools', 'far')
PM = os.path.join(common.SDK_ROOT, 'tools', 'pm')


def RunPackage(output_dir, target, package_path, package_name, run_args,
               symbolizer_config=None):
  """Copies the Fuchsia package at |package_path| to the target,
  executes it with |run_args|, and symbolizes its output.

  output_dir: The path containing the build output files.
  target: The deployment Target object that will run the package.
  package_path: The path to the .far package file.
  package_name: The name of app specified by package metadata.
  run_args: The arguments which will be passed to the Fuchsia process.
  symbolizer_config: A newline delimited list of source files contained
                     in the package. Omitting this parameter will disable
                     symbolization.

  Returns the exit code of the remote package process."""


  logging.debug('Copying package to target.')
  tmp_path = os.path.join('/tmp', os.path.basename(package_path))
  target.PutFile(package_path, tmp_path)

  logging.debug('Installing package.')
  p = target.RunCommandPiped(['pm', 'install', tmp_path],
                             stderr=subprocess.PIPE)
  output = p.stderr.readlines()
  p.wait()
  if p.returncode != 0:
    # Don't error out if the package already exists on the device.
    if len(output) != 1 or 'ErrAlreadyExists' not in output[0]:
      raise Exception('Error when installing package: %s' % '\n'.join(output))

  logging.debug('Running package.')
  command = ['run', package_name] + run_args
  process = target.RunCommandPiped(command,
                                   stdin=open(os.devnull, 'r'),
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT)

  if symbolizer_config:
    # Decorate the process output stream with the symbolizer.
    output = FilterStream(process.stdout, package_name,
                          symbolizer_config, output_dir)
  else:
    output = process.stdout

  for next_line in output:
    print next_line.strip()

  process.wait()
  if process.returncode != 0:
    # The test runner returns an error status code if *any* tests fail,
    # so we should proceed anyway.
    logging.warning('Command exited with non-zero status code %d.' %
                    process.returncode)

  return process.returncode
