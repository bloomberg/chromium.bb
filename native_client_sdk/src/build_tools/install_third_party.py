#! -*- python -*-
#
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Build and install all the third-party tools and libraries required to build
the SDK code.  To add a script, add it to the array |THIRD_PARTY_SCRIPTS|.
Before running the scripts, a couple of environment variables get set:
  PYTHONPATH - append this script's dir to the search path for module import.
  NACL_SDK_ROOT - forced to point to the root of this repo.
"""

import os
import subprocess
import sys

from optparse import OptionParser

# Append to PYTHONPATH in this very non-compliant way so that this script can be
# run from a DEPS hook, where the normal path rules don't apply.
SCRIPT_DIR = os.path.abspath(os.path.dirname(__file__))
THIRD_PARTY_DIR = os.path.join(os.path.dirname(SCRIPT_DIR), 'third_party')
SCONS_DIR = os.path.join(THIRD_PARTY_DIR, 'scons-2.0.1', 'engine')
sys.path.append(SCRIPT_DIR)
sys.path.append(SCONS_DIR)

import build_utils


THIRD_PARTY_SCRIPTS = [
    os.path.join('install_boost', 'install_boost.py'),
]


def main(argv):
  parser = OptionParser()
  parser.add_option(
      '-a', '--all-toolchains', dest='all_toolchains',
      action='store_true',
      help='Install into all available toolchains.')
  (options, args) = parser.parse_args(argv)
  if args:
    print 'ERROR: invalid argument: %s' % str(args)
    parser.print_help()
    sys.exit(1)

  python_paths = [SCRIPT_DIR, SCONS_DIR]
  shell_env = os.environ.copy()
  python_paths += [shell_env.get('PYTHONPATH', '')]
  shell_env['PYTHONPATH'] = os.pathsep.join(python_paths)

  # Force NACL_SDK_ROOT to point to the toolchain in this repo.
  nacl_sdk_root = os.path.dirname(SCRIPT_DIR)
  shell_env['NACL_SDK_ROOT'] = nacl_sdk_root

  script_argv = [arg for arg in argv if not arg in ['-a', '--all-toolchains']]
  if options.all_toolchains:
    script_argv += [
      '--toolchain=%s' % (
          build_utils.NormalizeToolchain(base_dir=nacl_sdk_root,
                                         arch='x86',
                                         variant='glibc')),
      '--toolchain=%s' % (
          build_utils.NormalizeToolchain(base_dir=nacl_sdk_root,
                                         arch='x86',
                                         variant='newlib')),
      '--third-party=%s' % THIRD_PARTY_DIR,
    ]

  for script in THIRD_PARTY_SCRIPTS:
    print "Running install script: %s" % os.path.join(SCRIPT_DIR, script)
    py_command = [sys.executable, os.path.join(SCRIPT_DIR, script)]
    subprocess.check_call(py_command + script_argv, env=shell_env)


if __name__ == '__main__':
  main(sys.argv[1:])
