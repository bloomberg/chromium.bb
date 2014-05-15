#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from buildbot_lib import (
    BuildContext, BuildStatus, ParseStandardCommandLine,
    RemoveSconsBuildDirectories, RemoveGypBuildDirectories, RunBuild, SCons,
    Step )


def SetupGypDefines(context, extra_vars=[]):
  context.SetEnv('GYP_DEFINES', ' '.join(context['gyp_vars'] + extra_vars))


# TODO(dschuff): These are cribbed directly from buildbot_standard.py.
# Factor into buildbot_lib? (Especially the MSVS part)
def SetupLinuxEnvironment(context):
  SetupGypDefines(context, ['target_arch=' + context['gyp_arch']])


def SetupMacEnvironment(context):
  SetupGypDefines(context)
  context.SetEnv('GYP_GENERATORS', 'ninja')


def SetupWindowsEnvironment(context):
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
      ('Microsoft Visual Studio 12.0', 'VS120COMNTOOLS', '2013'),
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


def BuildScriptX86(status, context):
  # Clean out build directories.
  with Step('clobber', status):
    RemoveSconsBuildDirectories()
    RemoveGypBuildDirectories()

  # Unlike their arm counterparts we do not run trusted tests on x86 bots.
  # Trusted tests get plenty of coverage by other bots, e.g. nacl-gcc bots.
  # We make the assumption here that there are no "exotic tests" which
  # are trusted in nature but are somehow depedent on the untrusted TC.
  flags_build = ['skip_trusted_tests=1', 'do_not_run_tests=1']
  flags_run = ['skip_trusted_tests=1']
  smoke_tests = ['small_tests', 'medium_tests']

  with Step('build_all', status):
    SCons(context, parallel=True, args=flags_build)

  # Normal pexe-mode tests
  with Step('smoke_tests', status, halt_on_fail=False):
    SCons(context, parallel=True, args=flags_run + smoke_tests)
  # Large tests cannot be run in parallel
  with Step('large_tests', status, halt_on_fail=False):
    SCons(context, parallel=False, args=flags_run + ['large_tests'])

  # non-pexe-mode tests. Build everything to make sure it all builds in nonpexe
  # mode, but just run the nonpexe_tests
  with Step('build_nonpexe', status):
    SCons(context, parallel=True, args=flags_build + ['pnacl_generate_pexe=0'])
  with Step('nonpexe_tests', status, halt_on_fail=False):
    SCons(context, parallel=True,
          args=flags_run + ['pnacl_generate_pexe=0', 'nonpexe_tests'])

  irt_mode = context['default_scons_mode'] + ['nacl_irt_test']
  smoke_tests_irt = ['small_tests_irt', 'medium_tests_irt']
  # Run some tests with the IRT
  with Step('smoke_tests_irt', status, halt_on_fail=False):
    SCons(context, parallel=True, mode=irt_mode,
          args=flags_run + smoke_tests_irt)

  # Test sandboxed translation
  if not context.Windows():
    # TODO(dschuff): The standalone sandboxed translator driver does not have
    # the batch script wrappers, so it can't run on Windows. Either add them to
    # the translator package or make SCons use the pnacl_newlib drivers except
    # on the ARM bots where we don't have the pnacl_newlib drivers.
    with Step('smoke_tests_sandboxed_translator', status, halt_on_fail=False):
      SCons(context, parallel=True, mode=irt_mode,
            args=flags_run + ['use_sandboxed_translator=1'] + smoke_tests_irt)
    with Step('smoke_tests_sandboxed_fast', status, halt_on_fail=False):
      SCons(context, parallel=True, mode=irt_mode,
            args=flags_run + ['use_sandboxed_translator=1', 'translate_fast=1']
            + smoke_tests_irt)

    # Translator memory consumption regression test
    with Step('large_code_test', status, halt_on_fail=False):
      SCons(context, parallel=True, mode=irt_mode,
            args=flags_run + ['use_sandboxed_translator=1', 'large_code'])

  # Test Non-SFI Mode.
  # The only architectures that the PNaCl toolchain supports Non-SFI
  # versions of are currently x86-32 and ARM, and ARM testing is covered
  # by buildbot_pnacl.sh rather than this Python script.
  if context.Linux() and context['default_scons_platform'] == 'x86-32':
    with Step('nonsfi_tests', status, halt_on_fail=False):
      # TODO(mseaborn): Enable more tests here when they pass.
      SCons(context, parallel=True, mode=irt_mode,
            args=flags_run + ['nonsfi_nacl=1', 'run_hello_world_test_irt'])


def Main():
  context = BuildContext()
  status = BuildStatus(context)
  ParseStandardCommandLine(context)

  if context.Linux():
    SetupLinuxEnvironment(context)
  elif context.Windows():
    SetupWindowsEnvironment(context)
  elif context.Mac():
    SetupMacEnvironment(context)
  else:
    raise Exception('Unsupported platform')

  RunBuild(BuildScriptX86, status)

if __name__ == '__main__':
  Main()
