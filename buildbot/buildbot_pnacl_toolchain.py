#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import platform
import subprocess
import sys

import buildbot_lib

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
NACL_DIR = os.path.dirname(SCRIPT_DIR)

# Put python_lib_dir at the front of the path to ensure that all platforms
# use the same argparse. If we add more modules, we should factor this into
# a call that just adds all the python_libs directories to the path.
python_lib_dir = os.path.join(os.path.dirname(NACL_DIR), 'third_party',
                              'python_libs', 'argparse')
sys.path.insert(0, python_lib_dir)
import argparse


# As this is a buildbot script, we want verbose logging. Note however, that
# toolchain_build has its own log settings, controlled by its CLI flags.
logging.getLogger().setLevel(logging.DEBUG)

host_os = buildbot_lib.GetHostPlatform()

# This is a minimal context, not useful for running tests yet, but enough for
# basic Step handling.
context = buildbot_lib.BuildContext()
status = buildbot_lib.BuildStatus(context)

with buildbot_lib.Step('Update cygwin/check bash', status, halt_on_fail=True):
  # Update cygwin if necessary.
  if host_os == 'win':
    if sys.platform == 'cygwin':
      print 'This script does not support running from inside cygwin!'
      sys.exit(1)
    subprocess.check_call(os.path.join(SCRIPT_DIR, 'cygwin_env.bat'))
    print os.environ['PATH']
    paths = os.environ['PATH'].split(os.pathsep)
    # Put path to cygwin tools at the beginning, so cygwin tools like python
    # and cmake will supercede others (which do not understand cygwin paths)
    paths = [os.path.join(NACL_DIR, 'cygwin', 'bin')] + paths
    print paths
    os.environ['PATH'] = os.pathsep.join(paths)
    print os.environ['PATH']
    bash = os.path.join(NACL_DIR, 'cygwin', 'bin', 'bash')
  else:
    # Assume bash is in the path
    bash = 'bash'

  try:
    print 'Bash version:'
    sys.stdout.flush()
    subprocess.check_call([bash , '--version'])
  except subprocess.CalledProcessError:
    print 'Bash not found in path!'
    raise buildbot_lib.StepFailed()

# Call toolchain_build_pnacl.py in trybot mode, since we aren't archiving
# anything but we want to see all the output. toolchain_build_pnacl outputs its
# own buildbot annotations.
try:
  command = [sys.executable,
             os.path.join(
                 NACL_DIR, 'toolchain_build', 'toolchain_build_pnacl.py'),
             '--trybot']
  logging.info('Running: ' + ' '.join(command))
  subprocess.check_call(command)
except subprocess.CalledProcessError:
  # Ignore any failures and keep going (but make the bot stage red).
  print '@@@STEP_FAILED@@@'
  sys.stdout.flush()


# Now we run the PNaCl buildbot script. It in turn runs the PNaCl build.sh
# script and runs scons tests.
# TODO(dschuff): re-implement the test-running portion of buildbot_pnacl.sh
# using buildbot_lib, and use them here and in the non-toolchain builder.
buildbot_shell = os.path.join(NACL_DIR, 'buildbot', 'buildbot_pnacl.sh')

# Because patching mangles the shell script on the trybots, fix it up here
# so we can have working windows trybots.
if host_os == 'win':
  with open(buildbot_shell, 'rb') as script:
    data = script.read()
  data = data.replace('\r\n', '\n')
  with open(buildbot_shell, 'wb') as script:
    script.write(data)

# Generate flags for buildbot_pnacl.sh
if host_os == 'linux':
  arg_os = 'linux'
  # TODO(dschuff): Figure out if it makes sense to import the utilities from
  # build/ into scripts from buildbot/ or only use things from buildbot_lib,
  # or unify them in some way.
  arch = 'x8664' if platform.machine() == 'x86_64' else 'x8632'
elif host_os == 'mac':
  arg_os = 'mac'
  arch = 'x8632'
elif host_os == 'win':
  arg_os = 'win'
  arch = 'x8664'
else:
  print 'Unrecognized platform: ', host_os
  sys.exit(1)

parser = argparse.ArgumentParser(description='PNaCl toolchain buildbot script')
group = parser.add_mutually_exclusive_group()
group.add_argument('--buildbot', action='store_true',
                 help='Buildbot mode (build and archive the toolchain)')
group.add_argument('--trybot', action='store_true',
                 help='Trybot mode (build but do not archove the toolchain)')
args = parser.parse_args()
if args.buildbot:
  trybot_mode = 'false'
elif args.trybot:
  trybot_mode = 'true'

platform_arg = 'mode-buildbot-tc-' + arch + '-' + arg_os

command = [bash,
           buildbot_shell,
           platform_arg,
           trybot_mode]
logging.info('Running: ' + ' '.join(command))
subprocess.check_call(command)
