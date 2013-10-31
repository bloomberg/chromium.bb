#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

from buildbot_lib import (
    BuildContext, BuildStatus, Command, ParseStandardCommandLine,
    RemoveSconsBuildDirectories, RemoveGypBuildDirectories, RunBuild, SCons,
    Step )


def SetupGypDefines(context, extra_vars=[]):
  context.SetEnv('GYP_DEFINES', ' '.join(context['gyp_vars'] + extra_vars))


def SetupLinuxEnvironment(context):
  SetupGypDefines(context, ['target_arch=' + context['gyp_arch']])


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
  with Step('smoke_tests_sandboxed_translator', status):
    SCons(context, parallel=True, mode=irt_mode,
          args=flags_run + ['use_sandboxed_translator=1'] + smoke_tests_irt)
  with Step('smoke_tests_sandboxed_fast', status):
    SCons(context, parallel=True, mode=irt_mode,
          args=flags_run + ['use_sandboxed_translator=1', 'translate_fast=1']
          + smoke_tests_irt)

  # Translator memory consumption regression test
  with Step('large_code_test', status):
    SCons(context, parallel=True, mode=irt_mode,
          args=flags_run + ['use_sandboxed_translator=1', 'large_code'])


def Main():
  context = BuildContext()
  status = BuildStatus(context)
  ParseStandardCommandLine(context)

  if context.Linux():
    SetupLinuxEnvironment(context)
  else:
    raise Exception('Unsupported platform')

  RunBuild(BuildScriptX86, status)

if __name__ == '__main__':
  Main()
