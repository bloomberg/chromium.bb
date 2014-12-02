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
parser.add_argument('--tests-arch', choices=['x86-32', 'x86-64'],
                    default='x86-64',
                    help='Host architecture for tests in buildbot_pnacl.sh')
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
                     '--verbose', '--clobber']

  if args.buildbot:
    executable_args.append('--buildbot')
  elif args.trybot:
    executable_args.append('--trybot')

  # Enabling LLVM assertions have a higher cost on Windows, particularly in the
  # presence of threads. So disable them on windows but leave them on elsewhere
  # to get the extra error checking.
  # See https://code.google.com/p/nativeclient/issues/detail?id=3830
  if host_os == 'win':
    executable_args.append('--disable-llvm-assertions')

  return executable + executable_args + sync_flag + extra_flags


# Clean out any installed toolchain parts that were built by previous bot runs.
with buildbot_lib.Step('Clobber TC install dir', status):
  print 'Removing', toolchain_install_dir
  pynacl.file_tools.RemoveDirectoryIfPresent(toolchain_install_dir)


# Run checkdeps so that the PNaCl toolchain trybots catch mistakes that would
# cause the normal NaCl bots to fail.
with buildbot_lib.Step('checkdeps', status):
  buildbot_lib.Command(
      context,
      [sys.executable,
       os.path.join(NACL_DIR, 'tools', 'checkdeps', 'checkdeps.py')])


if host_os != 'win':
  with buildbot_lib.Step('update clang', status):
    buildbot_lib.Command(
        context,
        [sys.executable,
         os.path.join(
             NACL_DIR, '..', 'tools', 'clang', 'scripts', 'update.py')])

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
# Always run with the system python.
# TODO(dschuff): remove support for cygwin python once the mingw build is rolled
cmd = ToolchainBuildCmd(None,
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

  # Extract packages for bot testing
  packages.ExtractPackages(TEMP_PACKAGES_FILE)

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
  with buildbot_lib.Step('LLVM Regression', status,
                         halt_on_fail=False):
    llvm_test = [sys.executable,
                 os.path.join(NACL_DIR, 'pnacl', 'scripts', 'llvm-test.py'),
                 '--llvm-regression',
                 '--verbose']
    buildbot_lib.Command(context, llvm_test)

sys.stdout.flush()
# On Linux we build all toolchain components (driven from this script), and then
# call buildbot_pnacl.sh which builds the sandboxed translator and runs tests
# for all the components.
# On Mac we build the toolchain but not the sandboxed translator, and run the
# same tests as the main waterfall bot (which also does not run the sandboxed
# translator: see https://code.google.com/p/nativeclient/issues/detail?id=3856 )
# On Windows we don't build the target libraries, so we can't run the SCons
# tests (other than the gold_plugin_test) on those platforms yet.
# For now full test coverage is only achieved on the main waterfall bots.
if host_os == 'win':
  sys.exit(0)
elif host_os == 'mac':
  subprocess.check_call([sys.executable,
                         os.path.join(NACL_DIR, 'buildbot','buildbot_pnacl.py'),
                         'opt', '64', 'pnacl'])
else:
  # Now we run the PNaCl buildbot script. It in turn runs the PNaCl build.sh
  # script (currently only for the sandboxed translator) and runs scons tests.
  # TODO(dschuff): re-implement the test-running portion of buildbot_pnacl.sh
  # using buildbot_lib, and use them here and in the non-toolchain builder.
  buildbot_shell = os.path.join(NACL_DIR, 'buildbot', 'buildbot_pnacl.sh')

  # Generate flags for buildbot_pnacl.sh

  arch = 'x8664' if args.tests_arch == 'x86-64' else 'x8632'

  if args.buildbot:
    trybot_mode = 'false'
  else:
    trybot_mode = 'true'

  platform_arg = 'mode-buildbot-tc-' + arch + '-linux'

  command = [bash, buildbot_shell, platform_arg,  trybot_mode]
  logging.info('Running: ' + ' '.join(command))
  subprocess.check_call(command)
