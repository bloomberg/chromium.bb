#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Enable 'with' statements in Python 2.5
from __future__ import with_statement

import os.path
import re
import subprocess
import sys
import time

from buildbot_lib import (
    BuildContext, BuildStatus, Command, EnsureDirectoryExists,
    ParseStandardCommandLine, RemoveDirectory, RunBuild, SCons, Step, StepLink,
    StepText, TryToCleanContents)


# Windows-specific environment manipulation
def SetupWindowsEnvironment(context):
  # Blow away path for now if on the bots (to be more hermetic).
  if os.environ.get('BUILDBOT_SLAVENAME'):
    paths = [
        r'c:\b\depot_tools',
        r'c:\b\depot_tools\python_bin',
        r'c:\b\build_internal\tools',
        r'e:\b\depot_tools',
        r'e:\b\depot_tools\python_bin',
        r'e:\b\build_internal\tools',
        r'C:\WINDOWS\system32',
        r'C:\WINDOWS\system32\WBEM',
        ]
    context.SetEnv('PATH', os.pathsep.join(paths))

  # Poke around looking for MSVC.  We should do something more principled in
  # the future.

  # The name of Program Files can differ, depending on the bittage of Windows.
  program_files = r'c:\Program Files (x86)'
  if not os.path.exists(program_files):
    program_files = r'c:\Program Files'
    if not os.path.exists(program_files):
      raise Exception('Cannot find the Program Files directory!')

  # The location of MSVC can differ depending on the version.
  msvc_locs = [
      ('Microsoft Visual Studio 10.0', 'VS100COMNTOOLS', '2010'),
      ('Microsoft Visual Studio 9.0', 'VS90COMNTOOLS', '2008'),
      ('Microsoft Visual Studio 8.0', 'VS80COMNTOOLS', '2005'),
  ]

  for dirname, comntools_var, gyp_msvs_version in msvc_locs:
    msvc = os.path.join(program_files, dirname)
    context.SetEnv('GYP_MSVS_VERSION', gyp_msvs_version)
    if os.path.exists(msvc):
      break
  else:
    # The break statement did not execute.
    raise Exception('Cannot find MSVC!')

  # Put MSVC in the path.
  vc = os.path.join(msvc, 'VC')
  comntools = os.path.join(msvc, 'Common7', 'Tools')
  perf = os.path.join(msvc, 'Team Tools', 'Performance Tools')
  context.SetEnv('PATH', os.pathsep.join([
      context.GetEnv('PATH'),
      vc,
      comntools,
      perf]))

  # SCons needs this variable to find vsvars.bat.
  # The end slash is needed because the batch files expect it.
  context.SetEnv(comntools_var, comntools + '\\')

  # This environment variable will SCons to print debug info while it searches
  # for MSVC.
  context.SetEnv('SCONS_MSCOMMON_DEBUG', '-')

  # Needed for finding devenv.
  context['msvc'] = msvc

  # The context on other systems has GYP_DEFINES set, set it for windows to be
  # able to save and restore without KeyError.
  context.SetEnv('GYP_DEFINES', '')


def SetupGypDefines(context, extra_vars=[]):
  context.SetEnv('GYP_DEFINES', ' '.join(context['gyp_vars'] + extra_vars))


def SetupLinuxEnvironment(context):
  SetupGypDefines(context, ['target_arch='+context['gyp_arch']])


def SetupMacEnvironment(context):
  SetupGypDefines(context)
  context.SetEnv('GYP_GENERATORS', 'ninja')


def SetupContextVars(context):
  # The branch is set to native_client on the main bots, on the trybots it's
  # set to ''.  Otherwise, we should assume a particular branch is being used.
  context['branch'] = os.environ.get('BUILDBOT_BRANCH', 'native_client')
  context['off_trunk'] = context['branch'] not in ['native_client', '']


def ValidatorTest(context, architecture, validator, warn_only=False):
  cmd=[
      sys.executable,
      'tests/abi_corpus/validator_regression_test.py',
      '--keep-going',
      '--validator', validator,
      '--arch', architecture
  ]
  if warn_only:
    cmd.append('--warn-only')
  Command(context, cmd=cmd)


def CommandGypBuild(context):
  if context.Windows():
    Command(
        context,
        cmd=[os.path.join(context['msvc'], 'Common7', 'IDE', 'devenv.com'),
             r'build\all.sln',
             '/build', context['gyp_mode']])
  elif context.Linux():
    Command(context, cmd=['make', '-C', '..', '-k',
                          '-j%d' % context['max_jobs'], 'V=1',
                          'BUILDTYPE=' + context['gyp_mode']])
  elif context.Mac():
    Command(context, cmd=[
        'ninja', '-k', '0', '-C', '../out/' + context['gyp_mode']])
  else:
    raise Exception('Unknown platform')


def CommandGypGenerate(context):
  Command(
          context,
          cmd=[
              sys.executable,
              'native_client/build/gyp_nacl',
              'native_client/build/all.gyp',
              ],
          cwd='..')


def CommandGclientRunhooks(context):
  if context.Windows():
    gclient = 'gclient.bat'
  else:
    gclient = 'gclient'
  print 'Running gclient runhooks...'
  print 'GYP_DEFINES=' + context.GetEnv('GYP_DEFINES')
  Command(context, cmd=[gclient, 'runhooks', '--force'])


def RemoveGypBuildDirectories():
  # Remove all directories on all platforms.  Overkill, but it allows for
  # straight-line code.
  # Windows
  RemoveDirectory('build/Debug')
  RemoveDirectory('build/Release')
  RemoveDirectory('build/Debug-Win32')
  RemoveDirectory('build/Release-Win32')
  RemoveDirectory('build/Debug-x64')
  RemoveDirectory('build/Release-x64')

  # Linux and Mac
  RemoveDirectory('hg')
  RemoveDirectory('../xcodebuild')
  RemoveDirectory('../sconsbuild')
  RemoveDirectory('../out')
  RemoveDirectory('src/third_party/nacl_sdk/arm-newlib')


def BuildScript(status, context):
  inside_toolchain = context['inside_toolchain']

  # Clean out build directories.
  with Step('clobber', status):
    RemoveDirectory(r'scons-out')
    RemoveGypBuildDirectories()

  with Step('cleanup_temp', status):
    # Picking out drive letter on which the build is happening so we can use
    # it for the temp directory.
    if context.Windows():
      build_drive = os.path.splitdrive(os.path.abspath(__file__))[0]
      tmp_dir = os.path.join(build_drive, os.path.sep + 'temp')
      context.SetEnv('TEMP', tmp_dir)
      context.SetEnv('TMP', tmp_dir)
    else:
      tmp_dir = '/tmp'
    print 'Making sure %s exists...' % tmp_dir
    EnsureDirectoryExists(tmp_dir)
    print 'Cleaning up the contents of %s...' % tmp_dir
    # Only delete files and directories like:
    # a) C:\temp\83C4.tmp
    # b) /tmp/.org.chromium.Chromium.EQrEzl
    file_name_re = re.compile(
        r'[\\/]([0-9a-fA-F]+\.tmp|\.org\.chrom\w+\.Chrom\w+\..+)$')
    file_name_filter = lambda fn: file_name_re.search(fn) is not None
    TryToCleanContents(tmp_dir, file_name_filter)

    # Mac has an additional temporary directory; clean it up.
    # TODO(bradnelson): Fix Mac Chromium so that these temp files are created
    #     with open() + unlink() so that they will not get left behind.
    if context.Mac():
      subprocess.call(
          "find /var/folders -name '.org.chromium.*' -exec rm -rfv '{}' ';'",
          shell=True)
      subprocess.call(
          "find /var/folders -name '.com.google.Chrome*' -exec rm -rfv '{}' ';'",
          shell=True)

  # Skip over hooks when run inside the toolchain build because
  # download_toolchains would overwrite the toolchain build.
  if inside_toolchain:
    with Step('gyp_generate_only', status):
      CommandGypGenerate(context)
  else:
    with Step('gclient_runhooks', status):
      CommandGclientRunhooks(context)

  if context['clang']:
    with Step('update_clang', status):
      Command(context, cmd=['../tools/clang/scripts/update.sh'])

  # Just build both bitages of validator and test for --validator mode.
  if context['validator']:
    with Step('build ncval-x86-32', status):
      SCons(context, platform='x86-32', parallel=True, args=['ncval'])
    with Step('build ncval-x86-64', status):
      SCons(context, platform='x86-64', parallel=True, args=['ncval'])

    with Step('clobber dfa_validator', status):
      Command(context, cmd=['rm', '-rf', 'dfa_validator'])
    with Step('clone dfa_validator', status):
      Command(context, cmd=[
          'git', 'clone',
          'git://github.com/mseaborn/x86-decoder.git', 'dfa_validator32'])
      Command(context, cmd=[
          'git', 'checkout', '1a5963fa48739c586d5bbd3d46d0a8a7f25112f2'],
          cwd='dfa_validator32')
      Command(context, cmd=[
          'git', 'clone',
          'git://github.com/mseaborn/x86-decoder.git', 'dfa_validator64'])
      Command(context, cmd=[
          'git', 'checkout', '6ffa36f44cafd2cdad37e1e27254c498030ff712'],
          cwd='dfa_validator64')
    with Step('build dfa_validator_32', status):
      Command(context, cmd=['make'], cwd='dfa_validator32')
    with Step('build dfa_validator_64', status):
      Command(context, cmd=['make'], cwd='dfa_validator64')

    with Step('build ragel_validator-32', status):
      SCons(context, platform='x86-32', parallel=True, args=['ncval_new'])
    with Step('build ragel_validator-64', status):
      SCons(context, platform='x86-64', parallel=True, args=['ncval_new'])

    with Step('predownload validator corpus', status):
      Command(context,
          cmd=[sys.executable,
               'tests/abi_corpus/validator_regression_test.py',
               '--download-only'])

    with Step('validator_regression_test current x86-32', status,
        halt_on_fail=False):
      ValidatorTest(
          context, 'x86-32', 'scons-out/opt-linux-x86-32/staging/ncval')
    with Step('validator_regression_test current x86-64', status,
        halt_on_fail=False):
      ValidatorTest(
          context, 'x86-64', 'scons-out/opt-linux-x86-64/staging/ncval')

    with Step('validator_regression_test dfa x86-32', status,
        halt_on_fail=False):
      ValidatorTest(
          context, 'x86-32', 'dfa_validator32/dfa_ncval', warn_only=True)
    with Step('validator_regression_test dfa x86-64', status,
        halt_on_fail=False):
      ValidatorTest(
          context, 'x86-64', 'dfa_validator64/dfa_ncval', warn_only=True)

    with Step('validator_regression_test ragel x86-32', status,
        halt_on_fail=False):
      ValidatorTest(
          context, 'x86-32',
          'scons-out/opt-linux-x86-32/staging/ncval_new')
    with Step('validator_regression_test ragel x86-64', status,
        halt_on_fail=False):
      ValidatorTest(
          context, 'x86-64',
          'scons-out/opt-linux-x86-64/staging/ncval_new')

    with Step('validator_diff_tests', status, halt_on_fail=False):
      SCons(context, args=['validator_diff_tests'])
    return

  # Run checkdeps script to vet #includes.
  with Step('checkdeps', status):
    Command(context, cmd=[sys.executable, 'tools/checkdeps/checkdeps.py'])

  # Make sure our Gyp build is working.
  if not context['no_gyp']:
    with Step('gyp_compile', status):
      CommandGypBuild(context)

  # The main compile step.
  with Step('scons_compile', status):
    SCons(context, parallel=True, args=[])

  ### BEGIN tests ###
  with Step('small_tests', status, halt_on_fail=False):
    SCons(context, args=['small_tests'])
  with Step('medium_tests', status, halt_on_fail=False):
    SCons(context, args=['medium_tests'])
  with Step('large_tests', status, halt_on_fail=False):
    SCons(context, args=['large_tests'])

  with Step('compile IRT tests', status):
    SCons(context, parallel=True, mode=['nacl_irt_test'])

  with Step('small_tests under IRT', status, halt_on_fail=False):
    SCons(context, mode=context['default_scons_mode'] + ['nacl_irt_test'],
          args=['small_tests_irt'])
  with Step('medium_tests under IRT', status, halt_on_fail=False):
    SCons(context, mode=context['default_scons_mode'] + ['nacl_irt_test'],
          args=['medium_tests_irt'])

  # TODO(eugenis): reenable this on clang/opt once the LLVM issue is fixed
  # http://code.google.com/p/nativeclient/issues/detail?id=2473
  bug2473 = (context['clang'] or context['asan']) and context['mode'] == 'opt'
  if context.Mac() and context['arch'] != 'arm' and not bug2473:
    # We don't run all tests on x86-64 Mac yet because it would slow
    # down the bots too much.  We just run a small set of tests that
    # have previously been fixed to pass, in order to prevent
    # regressions.
    # TODO(mseaborn): Remove this when we have bots dedicated to
    # testing x86-64 Mac.
    with Step('minimal x86-64 test', status, halt_on_fail=False):
      SCons(context, parallel=True, platform='x86-64',
            args=['exception_tests',
                  'run_hello_world_test',
                  'run_execute_data_test',
                  'run_nacl_signal_test',
                  'run_signal_frame_test',
                  'run_signal_handler_test',
                  'run_trusted_mmap_test'])

  ### END tests ###

  if not context['no_gyp']:
    # Build with ragel-based validator using GYP.
    gyp_defines_save = context.GetEnv('GYP_DEFINES')
    context.SetEnv('GYP_DEFINES',
                   ' '.join([gyp_defines_save, 'nacl_validator_ragel=1']))
    with Step('gyp_compile_ragel', status):
      # Clobber GYP build to recompile necessary files with new preprocessor macro
      # definitions.  It is done because some build systems (such as GNU Make,
      # MSBuild etc.) do not consider compiler arguments as a dependency.
      RemoveGypBuildDirectories()
      CommandGypGenerate(context)
      CommandGypBuild(context)
    context.SetEnv('GYP_DEFINES', gyp_defines_save)

  # Build with ragel-based validator using scons.
  with Step('scons_compile_ragel', status):
    SCons(context, parallel=True, args=['validator_ragel=1'])

  # Smoke tests for the R-DFA validator.
  with Step('validator_ragel_tests', status):
    args = ['validator_ragel=1',
            'small_tests',
            'medium_tests',
            'large_tests',
            ]
    # Add nacl_irt_test mode to be able to run run_hello_world_test_irt that
    # tests validation of the IRT.
    SCons(context,
          mode=context['default_scons_mode'] + ['nacl_irt_test'],
          args=args)


def Main():
  # TODO(ncbray) make buildbot scripts composable to support toolchain use case.
  context = BuildContext()
  status = BuildStatus(context)
  ParseStandardCommandLine(context)
  SetupContextVars(context)
  if context.Windows():
    SetupWindowsEnvironment(context)
  elif context.Linux():
    SetupLinuxEnvironment(context)
  elif context.Mac():
    SetupMacEnvironment(context)
  else:
    raise Exception("Unsupported platform.")
  RunBuild(BuildScript, status)


def TimedMain():
  start_time = time.time()
  try:
    Main()
  finally:
    time_taken = time.time() - start_time
    print 'RESULT BuildbotTime: total= %.3f minutes' % (time_taken / 60)


if __name__ == '__main__':
  TimedMain()
