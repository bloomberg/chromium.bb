# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Contains a helper function for deploying and executing a packaged
executable on a Target."""

import logging
import os
import subprocess
import uuid

from symbolizer import FilterStream

def RunPackage(output_dir, target, package_path, run_args, symbolizer_config=None):
  """Copies the Fuchsia package at |package_path| to the target,
  executes it with |run_args|, and symbolizes its output.

  output_dir: The path containing the build output files.
  target: The deployment Target object that will run the package.
  package_path: The path to the .far package file.
  run_args: The command-linearguments which will be passed to the Fuchsia process.
  symbolizer_config: A newline delimited list of source files contained in the
                     package. Omitting this parameter will disable symbolization.

  Returns the exit code of the remote package process."""

  if symbolizer_config:
    if logging.getLogger().getEffectiveLevel() == logging.DEBUG:
      logging.debug('Contents of package "%s":' % os.path.basename(package_path))
      for next_line in open(symbolizer_config, 'r'):
        logging.debug('  ' + next_line.strip().split('=')[0])
      logging.debug('')

  # Copy the package.
  deployed_package_path = '/tmp/package-%s.far' % uuid.uuid1()
  target.PutFile(package_path, deployed_package_path)

  try:
    command = ['run', deployed_package_path] + run_args
    process = target.RunCommandPiped(command,
                                     stdin=open(os.devnull, 'r'),
                                     stdout=subprocess.PIPE)
    if symbolizer_config:
      # Decorate the process output stream with the symbolizer.
      output = FilterStream(process.stdout, symbolizer_config, output_dir)
    else:
      output = process.stdout

    for next_line in output:
      print next_line

    process.wait()
    if process.returncode != 0:
      # The test runner returns an error status code if *any* tests fail,
      # so we should proceed anyway.
      logging.warning('Command exited with non-zero status code %d.' %
                      process.returncode)
  finally:
    logging.debug('Cleaning up package file.')
    target.RunCommand(['rm', deployed_package_path])

  return process.returncode
