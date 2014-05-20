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
import pynacl.file_tools

import buildbot_lib
import packages

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
NACL_DIR = os.path.dirname(SCRIPT_DIR)
TOOLCHAIN_BUILD_DIR = os.path.join(NACL_DIR, 'toolchain_build')
TOOLCHAIN_BUILD_OUT_DIR = os.path.join(TOOLCHAIN_BUILD_DIR, 'out')

TEMP_PACKAGES_FILE = os.path.join(TOOLCHAIN_BUILD_OUT_DIR, 'packages.txt')

BUILD_DIR = os.path.join(NACL_DIR, 'build')
PACKAGE_VERSION_DIR = os.path.join(BUILD_DIR, 'package_version')
PACKAGE_VERSION_SCRIPT = os.path.join(PACKAGE_VERSION_DIR, 'package_version.py')

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
buildbot_lib.SetDefaultContextAttributes(context)
context['pnacl'] = True
status = buildbot_lib.BuildStatus(context)

toolchain_install_dir = os.path.join(
    NACL_DIR,
    'toolchain',
    '%s_%s' % (host_os, pynacl.platform.GetArch()),
    'pnacl_newlib')

def ToolchainBuildCmd(python_executable=None, sync=False, extra_flags=[]):
  executable = [python_executable] if python_executable else [sys.executable]
  sync_flag = ['--sync'] if sync else []

  # The path to the script is a relative path with forward slashes so it is
  # interpreted properly when it uses __file__ inside cygwin
  executable_args = ['toolchain_build/toolchain_build_pnacl.py',
                     '--verbose', '--clobber', '--build-64bit-host',
                     '--install', toolchain_install_dir]

  if args.buildbot:
    executable_args.append('--buildbot')
  elif args.trybot:
    executable_args.append('--trybot')

  return executable + executable_args + sync_flag + extra_flags


# Sync the git repos used by build.sh
with buildbot_lib.Step('Sync build.sh repos', status, halt_on_fail=True):
  buildbot_lib.Command(
    context, ToolchainBuildCmd(extra_flags=['--legacy-repo-sync']))

# Clean out any installed toolchain parts that were built by previous bot runs.
with buildbot_lib.Step('Sync TC install dir', status):
  pynacl.file_tools.RemoveDirectoryIfPresent(toolchain_install_dir)
  buildbot_lib.Command(
      context,
      [sys.executable, PACKAGE_VERSION_SCRIPT,
       '--packages', 'pnacl_newlib', 'sync', '--extract'])

# Run checkdeps so that the PNaCl toolchain trybots catch mistakes that would
# cause the normal NaCl bots to fail.
with buildbot_lib.Step('checkdeps', status):
  buildbot_lib.Command(
      context,
      [sys.executable,
       os.path.join(NACL_DIR, 'tools', 'checkdeps', 'checkdeps.py')])


if host_os == 'win':
  # On windows, sync with Windows git/svn rather than cygwin git/svn
  with buildbot_lib.Step('Sync toolchain_build sources', status):
    buildbot_lib.Command(
      context, ToolchainBuildCmd(sync=True, extra_flags=['--sync-only']))

with buildbot_lib.Step('Update cygwin/check bash', status, halt_on_fail=True):
  # Update cygwin if necessary.
  if host_os == 'win':
    if sys.platform == 'cygwin':
      print 'This script does not support running from inside cygwin!'
      sys.exit(1)
      subprocess.check_call(os.path.join(SCRIPT_DIR, 'cygwin_env.bat'))
    saved_path = os.environ['PATH']
    print saved_path
    paths = saved_path.split(os.pathsep)
    # Put path to cygwin tools at the beginning, so cygwin tools like python
    # and cmake will supercede others (which do not understand cygwin paths)
    paths = [os.path.join(NACL_DIR, 'cygwin', 'bin')] + paths
    print paths
    os.environ['PATH'] = os.pathsep.join(paths)
    print os.environ['PATH']
    bash = os.path.join(NACL_DIR, 'cygwin', 'bin', 'bash')
    cygwin_python = os.path.join(NACL_DIR, 'cygwin', 'bin', 'python')
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

# toolchain_build outputs its own buildbot annotations, so don't use
# buildbot_lib.Step to run it here.
try:
  cmd = ToolchainBuildCmd(cygwin_python if host_os == 'win' else None,
                          host_os != 'win', # On Windows, we synced already
                          ['--packages-file', TEMP_PACKAGES_FILE])
  logging.info('Running: ' + ' '.join(cmd))
  subprocess.check_call(cmd)

  if args.buildbot or args.trybot:
    # Don't upload packages from the 32-bit linux bot to avoid racing on
    # uploading the same packages as the 64-bit linux bot
    if host_os != 'linux' or pynacl.platform.IsArch64Bit():
      if host_os == 'win':
        # Since we are currently running the build in cygwin, the filenames in
        # TEMP_PACKAGES_FILE will have cygwin paths. Convert them to system
        # paths so we dont' have to worry about running package_version tools
        # in cygwin.
        converted = []
        with open(TEMP_PACKAGES_FILE) as f:
          for line in f:
            converted.append(
              subprocess.check_output(['cygpath', '-w', line]).strip())
        with open(TEMP_PACKAGES_FILE, 'w') as f:
          f.write('\n'.join(converted))
      packages.UploadPackages(TEMP_PACKAGES_FILE, args.trybot)

except subprocess.CalledProcessError:
  # Ignore any failures and keep going (but make the bot stage red).
  print '@@@STEP_FAILURE@@@'
sys.stdout.flush()

# Since mac and windows bots don't build target libraries or run tests yet,
# Run a basic sanity check that tests the host components (LLVM, binutils,
# gold plugin)
if host_os == 'win' or host_os == 'mac':
  with buildbot_lib.Step('Test host binaries and gold plugin', status,
                         halt_on_fail=False):
    buildbot_lib.Command(
        context,
        [sys.executable,
        os.path.join('tests', 'gold_plugin', 'gold_plugin_test.py'),
        '--toolchaindir', toolchain_install_dir])

if host_os != 'win':
  # TODO(dschuff): Fix windows regression test runner (upstream in the LLVM
  # codebase or locally in the way we build LLVM) ASAP
  with buildbot_lib.Step('LLVM Regression (toolchain_build)', status):
    llvm_test = [sys.executable,
                 os.path.join(NACL_DIR, 'pnacl', 'scripts', 'llvm-test.py'),
                 '--llvm-regression',
                 '--verbose']
    buildbot_lib.Command(context, llvm_test)


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
