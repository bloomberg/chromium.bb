#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import atexit
import logging
import os
import shutil
import sys
import tempfile

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir, os.pardir,
                             os.pardir, 'build', 'android'))
from pylib import constants

sys.path.insert(0, os.path.join(os.path.abspath(os.path.dirname(__file__)),
                                os.pardir, os.pardir, os.pardir, 'mojo',
                                'tools'))

from mopy.android import AndroidShell
from mopy.config import Config

USAGE = ('run_mandoline.py [<shell-and-app-args>] [<start-page-url>]')

def _CreateSOLinks(dest_dir):
  '''Creates links from files (eg. *.mojo) to the real .so for gdb to find.'''
  # The files to create links for. The key is the name as seen on the device,
  # and the target an array of path elements as to where the .so lives (relative
  # to the output directory).
  # TODO(sky): come up with some way to automate this.
  files_to_link = {
    'html_viewer.mojo': ['libhtml_viewer_library.so'],
    'libmandoline_runner.so': ['mandoline_runner'],
  }
  build_dir = constants.GetOutDirectory()
  print build_dir
  for android_name, so_path in files_to_link.iteritems():
    src = os.path.join(build_dir, *so_path)
    if not os.path.isfile(src):
      print '*** Expected file not found', src
      print '*** Aborting launch.'
      sys.exit(-1)
    os.symlink(src, os.path.join(dest_dir, android_name))


def main():
  logging.basicConfig()

  parser = argparse.ArgumentParser(usage=USAGE)

  debug_group = parser.add_mutually_exclusive_group()
  debug_group.add_argument('--debug', help='Debug build (default)',
                           default=True, action='store_true')
  debug_group.add_argument('--release', help='Release build', default=False,
                           dest='debug', action='store_false')
  parser.add_argument('--build-dir', help='Build directory')
  parser.add_argument('--target-cpu', help='CPU architecture to run for.',
                      choices=['x64', 'x86', 'arm'], default='arm')
  parser.add_argument('--device', help='Serial number of the target device.')
  parser.add_argument('--gdb', help='Run gdb',
                      default=False, action='store_true')
  runner_args, args = parser.parse_known_args()

  config = Config(build_dir=runner_args.build_dir,
                  target_os=Config.OS_ANDROID,
                  target_cpu=runner_args.target_cpu,
                  is_debug=runner_args.debug,
                  apk_name='Mandoline.apk')
  shell = AndroidShell(config)
  shell.InitShell(runner_args.device)
  p = shell.ShowLogs()

  temp_gdb_dir = None
  if runner_args.gdb:
    temp_gdb_dir = tempfile.mkdtemp()
    atexit.register(shutil.rmtree, temp_gdb_dir, True)
    _CreateSOLinks(temp_gdb_dir)

  shell.StartActivity('MandolineActivity',
                      args,
                      sys.stdout,
                      p.terminate,
                      temp_gdb_dir)
  return 0


if __name__ == '__main__':
  sys.exit(main())
