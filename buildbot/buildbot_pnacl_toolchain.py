#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import os
import platform
import subprocess
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
import pynacl.platform

import buildbot_lib
import packages

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
NACL_DIR = os.path.dirname(SCRIPT_DIR)
BUILD_DIR = os.path.join(NACL_DIR, 'build')
PACKAGE_VERSION_DIR = os.path.join(BUILD_DIR, 'package_version')
TOOLCHAIN_BUILD_DIR = os.path.join(NACL_DIR, 'toolchain_build')
TOOLCHAIN_BUILD_OUT_DIR = os.path.join(TOOLCHAIN_BUILD_DIR, 'out')

PACKAGE_VERSION_SCRIPT = os.path.join(PACKAGE_VERSION_DIR, 'package_version.py')
TEMP_PACKAGES_FILE = os.path.join(TOOLCHAIN_BUILD_OUT_DIR, 'packages.txt')

# As this is a buildbot script, we want verbose logging. Note however, that
# toolchain_build has its own log settings, controlled by its CLI flags.
logging.getLogger().setLevel(logging.DEBUG)

parser = argparse.ArgumentParser(description='PNaCl toolchain buildbot script')
group = parser.add_mutually_exclusive_group()
group.add_argument('--buildbot', action='store_true',
                 help='Buildbot mode (build and archive the toolchain)')
group.add_argument('--trybot', action='store_true',
                 help='Trybot mode (build but do not archove the toolchain)')
args = parser.parse_args()

host_os = buildbot_lib.GetHostPlatform()

# This is a minimal context, not useful for running tests yet, but enough for
# basic Step handling.
context = buildbot_lib.BuildContext()
status = buildbot_lib.BuildStatus(context)


toolchain_build_cmd = [
    sys.executable,
    os.path.join(
        NACL_DIR, 'toolchain_build', 'toolchain_build_pnacl.py'),
    '--verbose', '--sync', '--clobber', '--build-64bit-host']

# Sync the git repos used by build.sh
with buildbot_lib.Step('Sync build.sh repos', status, halt_on_fail=True):
  buildbot_lib.Command(context, toolchain_build_cmd + ['--legacy-repo-sync'])

# Run toolchain_build.py first. Its outputs are not actually being used yet.
# toolchain_build outputs its own buildbot annotations, so don't use
# buildbot_lib.Step to run it here.
try:
  gsd_arg = []
  if args.buildbot:
    gsd_arg = ['--buildbot']
  elif args.trybot:
    gsd_arg = ['--trybot']

  cmd = toolchain_build_cmd + gsd_arg + ['--packages-file', TEMP_PACKAGES_FILE]
  logging.info('Running: ' + ' '.join(cmd))
  subprocess.check_call(cmd)

  if args.buildbot or args.trybot:
    print '@@@BUILD_STEP upload_package_info@@@'
    packages.UploadPackages(TEMP_PACKAGES_FILE, args.trybot)
    sys.stdout.flush()

except subprocess.CalledProcessError:
  # Ignore any failures and keep going (but make the bot stage red).
  print '@@@STEP_FAILURE@@@'
sys.stdout.flush()

if host_os != 'win':
  # TODO(dschuff): Fix windows regression test runner (upstream in the LLVM
  # codebase or locally in the way we build LLVM) ASAP
  with buildbot_lib.Step('LLVM Regression (toolchain_build)', status):
    llvm_test = [sys.executable,
                 os.path.join(NACL_DIR, 'pnacl', 'scripts', 'llvm-test.py'),
                 '--llvm-regression',
                 '--verbose']
    buildbot_lib.Command(context, llvm_test)

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

# Now we run the PNaCl buildbot script. It in turn runs the PNaCl build.sh
# script and runs scons tests.
# TODO(dschuff): re-implement the test-running portion of buildbot_pnacl.sh
# using buildbot_lib, and use them here and in the non-toolchain builder.
buildbot_shell = os.path.join(NACL_DIR, 'buildbot', 'buildbot_pnacl.sh')

# Because patching mangles the shell script on the trybots, fix it up here
# so we can have working windows trybots.
def FixCRLF(f):
  with open(f, 'rb') as script:
    data = script.read().replace('\r\n', '\n')
  with open(f, 'wb') as script:
    script.write(data)

if host_os == 'win':
  FixCRLF(buildbot_shell)
  FixCRLF(os.path.join(NACL_DIR, 'pnacl', 'build.sh'))
  FixCRLF(os.path.join(NACL_DIR, 'pnacl', 'scripts', 'common-tools.sh'))

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
