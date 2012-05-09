#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Enable 'with' statements in Python 2.5
from __future__ import with_statement

import os.path
import re
import sys

from buildbot_lib import (
    BuildContext, BuildStatus, Command, EnsureDirectoryExists,
    ParseStandardCommandLine, RemoveDirectory, RunBuild, SCons, Step, StepLink,
    StepText, TryToCleanContents)


# Windows-specific environment manipulation
def SetupWindowsEnvironment(context):
  context.SetEnv('GYP_MSVS_VERSION', '2008')

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
      # TODO(ncbray) use msvc 10.  This will require changing GYP_MSVS_VERSION.
      #('Microsoft Visual Studio 10.0', 'VS100COMNTOOLS'),
      ('Microsoft Visual Studio 9.0', 'VS90COMNTOOLS'),
      ('Microsoft Visual Studio 8.0', 'VS80COMNTOOLS'),
  ]

  for dirname, comntools_var in msvc_locs:
    msvc = os.path.join(program_files, dirname)
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


def SetupGypDefines(context, extra_vars=[]):
  context.SetEnv('GYP_DEFINES', ' '.join(context['gyp_vars'] + extra_vars))


def SetupLinuxEnvironment(context):
  SetupGypDefines(context, ['target_arch='+context['gyp_arch']])


def SetupMacEnvironment(context):
  SetupGypDefines(context)


def SetupContextVars(context):
  # The branch is set to native_client on the main bots, on the trybots it's
  # set to ''.  Otherwise, we should assume a particular branch is being used.
  context['branch'] = os.environ.get('BUILDBOT_BRANCH', 'native_client')
  context['off_trunk'] = context['branch'] not in ['native_client', '']


def ValidatorTest(context, architecture, validator):
  Command(context,
      cmd=[sys.executable,
        'tests/abi_corpus/validator_regression_test.py',
        '--keep-going',
        '--validator', validator,
        '--arch', architecture])


def BuildScript(status, context):
  inside_toolchain = context['inside_toolchain']
  # When off the trunk, we don't have anywhere to get Chrome binaries
  # from the appropriate branch, so we can't test the right Chrome.
  do_integration_tests = (not inside_toolchain and
                          not context['off_trunk'] and
                          not context['asan'])

  # If we're running browser tests on a 64-bit Windows machine, build a 32-bit
  # plugin.
  need_plugin_32 = (context.Windows() and
                    context['bits'] == '64' and
                    not inside_toolchain)

  # Clean out build directories.
  # Remove all directories on all platforms.  Overkill, but it allows for
  # straight-line code.
  with Step('clobber', status):
    RemoveDirectory(r'scons-out')

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

  # Skip over hooks when run inside the toolchain build because
  # download_toolchains would overwrite the toolchain build.
  if inside_toolchain:
    with Step('gyp_generate_only', status):
      Command(
          context,
          cmd=[
              sys.executable,
              'native_client/build/gyp_nacl',
              'native_client/build/all.gyp',
              ],
          cwd='..')
  else:
    with Step('gclient_runhooks', status):
      if context.Windows():
        gclient = 'gclient.bat'
      else:
        gclient = 'gclient'
      Command(context, cmd=[gclient, 'runhooks', '--force'])

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
      SCons(context, platform='x86-32', parallel=True, args=['validator-test'])
    with Step('build ragel_validator-64', status):
      SCons(context, platform='x86-64', parallel=True, args=['validator-test'])

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
          context, 'x86-32', 'dfa_validator32/dfa_ncval')
    with Step('validator_regression_test dfa x86-64', status,
        halt_on_fail=False):
      ValidatorTest(
          context, 'x86-64', 'dfa_validator64/dfa_ncval')

    with Step('validator_regression_test ragel x86-32', status,
        halt_on_fail=False):
      ValidatorTest(
          context, 'x86-32',
          'scons-out/opt-linux-x86-32/staging/validator-test')
    with Step('validator_regression_test ragel x86-64', status,
        halt_on_fail=False):
      ValidatorTest(
          context, 'x86-64',
          'scons-out/opt-linux-x86-64/staging/validator-test')
    return

  # Make sure our Gyp build is working.
  with Step('gyp_compile', status):
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
      Command(context, cmd=['xcodebuild', '-project', 'build/all.xcodeproj',
                            '-parallelizeTargets',
                            '-configuration', context['gyp_mode']])
    else:
      raise Exception('Unknown platform')

  # The main compile step.
  with Step('scons_compile', status):
    SCons(context, parallel=True, args=[])

  if need_plugin_32:
    with Step('plugin_compile_32', status):
      SCons(context, platform='x86-32', parallel=True, args=['plugin'])

  with Step('compile IRT tests', status):
    SCons(context, parallel=True, mode=['nacl_irt_test'])

  ### BEGIN tests ###
  with Step('small_tests', status, halt_on_fail=False):
    SCons(context, args=['small_tests'])
  with Step('medium_tests', status, halt_on_fail=False):
    SCons(context, args=['medium_tests'])
  with Step('large_tests', status, halt_on_fail=False):
    SCons(context, args=['large_tests'])

  if do_integration_tests:
    with Step('chrome_browser_tests', status, halt_on_fail=False):
      # Note that we have to add nacl_irt_test to --mode in order to
      # get inbrowser_test_runner to run.
      # TODO(mseaborn): Change it so that inbrowser_test_runner is not
      # a special case.
      SCons(context, browser_test=True,
            mode=context['default_scons_mode'] + ['nacl_irt_test'],
            args=['SILENT=1', 'chrome_browser_tests'])

    with Step('pyauto_tests', status, halt_on_fail=False):
      SCons(context, browser_test=True, args=['SILENT=1', 'pyauto_tests'])

  # IRT is incompatible with glibc startup hacks.
  # See http://code.google.com/p/nativeclient/issues/detail?id=2092
  if not context['use_glibc']:
    with Step('small_tests under IRT', status, halt_on_fail=False):
      SCons(context, mode=context['default_scons_mode'] + ['nacl_irt_test'],
            args=['small_tests_irt'])

    with Step('medium_tests under IRT', status, halt_on_fail=False):
      SCons(context, mode=context['default_scons_mode'] + ['nacl_irt_test'],
            args=['medium_tests_irt'])

  # TODO(eugenis): reenable this on clang/opt once the LLVM issue is fixed
  # http://code.google.com/p/nativeclient/issues/detail?id=2473
  bug2473 = (context['clang'] or context['asan']) and context['mode'] == 'opt'
  if context.Mac() and not bug2473:
    # x86-64 is not fully supported on Mac.  Not everything works, but we
    # want to stop x86-64 sel_ldr from regressing, so do a minimal test here.
    with Step('minimal x86-64 test', status, halt_on_fail=False):
      SCons(context, parallel=True, platform='x86-64',
            args=['run_hello_world_test'])

  ### END tests ###


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


if __name__ == '__main__':
  Main()
