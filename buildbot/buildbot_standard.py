#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Enable 'with' statements in Python 2.5
from __future__ import with_statement

import os.path
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


def SetupLinuxEnvironment(context):
  context.SetEnv('GYP_DEFINES', 'target_arch='+context['gyp_arch'])


def SetupMacEnvironment(context):
  pass


def UploadIrtBinary(status, context, branch):
  if context.Linux():
    with Step('archive irt.nexe', status):
      # Upload the integrated runtime (IRT) library so that it can be pulled
      # into the Chromium build, so that a NaCl toolchain will not be needed
      # to build a NaCl-enabled Chromium.  We strip the IRT to save space
      # and download time.
      # TODO(mseaborn): It might be better to do the stripping in Scons.
      irt_path = 'scons-out/nacl_irt-x86-%s/staging/irt.nexe' % context['bits']
      stripped_irt_path = irt_path + '.stripped'

      if os.environ.get('ARCHIVE_IRT') == '1':
        Command(
          context,
          cmd=['toolchain/linux_x86_newlib/bin/nacl-strip',
               '--strip-debug',
               irt_path,
               '-o', stripped_irt_path])

        irt_dir = 'nativeclient-archive2/irt'
        if branch != 'native_client':
          irt_dir = '%s/branches/%s' % (irt_dir, branch)
        gsdview = 'http://gsdview.appspot.com'
        rev = os.environ['BUILDBOT_GOT_REVISION']
        gs_path = '%s/r%s/irt_x86_%s.nexe' % (irt_dir, rev, context['bits'])

        def GSCPCommand(context, cmd):
          gs_util = '/b/build/scripts/slave/gsutil'
          Command(context, cmd=[gs_util, '-h', 'Cache-Control:no-cache', 'cp',
                                '-a', 'public-read'] + cmd)

        # Upload the stripped IRT
        GSCPCommand(context, cmd=[stripped_irt_path, 'gs://' + gs_path])
        StepLink('stripped', '/'.join([gsdview, gs_path]))

        # Upload the unstripped IRT, in case it's needed for debugging.
        GSCPCommand(context, cmd=[irt_path, 'gs://' + gs_path + '.unstripped'])
        StepLink('unstripped', '/'.join([gsdview, gs_path]) + '.unstripped')
      else:
        StepText('not uploading on this bot')


def BuildScript(status, context):
  branch = os.environ.get('BUILDBOT_BRANCH', 'native_client')
  inside_toolchain = context['inside_toolchain']
  do_integration_tests = (
      not context['use_glibc'] and not inside_toolchain and
      branch == 'native_client')
  do_dso_tests = (
      context['use_glibc'] and not inside_toolchain and
      branch == 'native_client')


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
    TryToCleanContents(tmp_dir)

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

  # Make sure out Gyp build is working.
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

  UploadIrtBinary(status, context, branch)

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
      SCons(context, browser_test=True,
            args=['SILENT=1', 'chrome_browser_tests'])

    # Use SCons to test the binaries built by GYP.
    with Step('chrome_browser_tests using GYP', status, halt_on_fail=False):
      if context.Windows():
        base = os.path.join('build', context['gyp_mode'])
        plugin = os.path.join(base, 'ppGoogleNaClPlugin.dll')
        if context['bits'] == '32':
          sel_ldr = os.path.join(base, 'sel_ldr.exe')
        else:
          sel_ldr = os.path.join(base, 'sel_ldr64.exe')
      elif context.Linux():
        base = os.path.join('..', 'out', context['gyp_mode'])
        plugin = os.path.join(base, 'lib.target/libppGoogleNaClPlugin.so')
        sel_ldr = os.path.join(base, 'sel_ldr')
      elif context.Mac():
        base = os.path.join('..', 'xcodebuild', context['gyp_mode'])
        plugin = os.path.join(base, 'libppGoogleNaClPlugin.dylib')
        sel_ldr = os.path.join(base, 'sel_ldr')
      else:
        raise Exception('Unknown platform')

      SCons(
          context,
          browser_test=True,
          args=['force_ppapi_plugin=' + plugin,
                'force_sel_ldr=' + sel_ldr,
                'SILENT=1',
                'chrome_browser_tests'])

    with Step('pyauto_tests', status, halt_on_fail=False):
      SCons(context, browser_test=True, args=['SILENT=1', 'pyauto_tests'])

  if do_dso_tests:
    with Step('dynamic_library_browser_tests', status, halt_on_fail=False):
      SCons(context, browser_test=True,
            args=['SILENT=1', 'dynamic_library_browser_tests'])

  # IRT is incompatible with glibc startup hacks.
  # See http://code.google.com/p/nativeclient/issues/detail?id=2092
  if not context['use_glibc']:
    with Step('small_tests under IRT', status, halt_on_fail=False):
      SCons(context, mode=context['default_scons_mode'] + ['nacl_irt_test'],
            args=['small_tests_irt'])

    with Step('medium_tests under IRT', status, halt_on_fail=False):
      SCons(context, mode=context['default_scons_mode'] + ['nacl_irt_test'],
            args=['medium_tests_irt'])

  if context.Mac():
    # x86-64 is not fully supported on Mac.  Not everything works, but we
    # want to stop x86-64 sel_ldr from regressing, so do a minimal test here.
    with Step('minimal x86-64 test', status, halt_on_fail=False):
      SCons(context, parallel=True, platform='x86-64',
            args=['run_hello_world_test'])

  ### END tests ###


def Main():
  # TODO(ncbray) make buildbot scripts composable to support toolchain use case.
  status = BuildStatus()
  context = BuildContext()
  ParseStandardCommandLine(context)
  if context.Windows():
    SetupWindowsEnvironment(context)
  elif context.Linux():
    SetupLinuxEnvironment(context)
  elif context.Mac():
    SetupMacEnvironment(context)
  else:
    raise Exception("Unsupported platform.")
  RunBuild(BuildScript, status, context)


if __name__ == '__main__':
  Main()
