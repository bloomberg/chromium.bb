#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from buildbot_lib import (
    BuildContext, BuildStatus, ParseStandardCommandLine,
    RemoveSconsBuildDirectories, RemoveGypBuildDirectories, RunBuild,
    SetupLinuxEnvironment, SetupMacEnvironment, SetupWindowsEnvironment, SCons,
    Step )



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
  # The x86-64 toolchain bot currently also runs these tests from
  # buildbot_pnacl.sh
  if context.Linux() and context['default_scons_platform'] == 'x86-32':
    with Step('nonsfi_tests', status, halt_on_fail=False):
      # TODO(mseaborn): Enable more tests here when they pass.
      tests = ['run_' + test + '_test_irt' for test in
               ['hello_world', 'float', 'malloc_realloc_calloc_free']]
      # Extra non-IRT-using test to run for x86-32
      tests.extend(['run_clock_get_test',
                    'run_dup_test',
                    'run_fork_test',
                    'run_hello_world_test',
                    'run_mmap_test',
                    'run_nanosleep_test',
                    'run_printf_test',
                    'run_pwrite_test',
                    'run_thread_test'])
      SCons(context, parallel=True, mode=irt_mode,
            args=flags_run + ['nonsfi_nacl=1'] + tests)

    # Test nonsfi_loader linked against host's libc.
    with Step('nonsfi_tests_host_libc', status, halt_on_fail=False):
      tests = ['run_' + test + '_test_irt' for test in
               ['hello_world', 'float', 'malloc_realloc_calloc_free',
                'dup', 'syscall', 'getpid']]
      # Using skip_nonstable_bitcode=1 here disables the tests for
      # zero-cost C++ exception handling, which don't pass for Non-SFI
      # mode yet because we don't build libgcc_eh for Non-SFI mode.
      tests.extend(['toolchain_tests_irt',
                    'skip_nonstable_bitcode=1'])
      SCons(context, parallel=True, mode=irt_mode,
            args=(flags_run + ['nonsfi_nacl=1', 'use_newlib_nonsfi_loader=0'] +
                  tests))

  # Test unsandboxed mode.
  if ((context.Linux() or context.Mac()) and
      context['default_scons_platform'] == 'x86-32'):
    if context.Linux():
      tests = ['run_' + test + '_test_irt' for test in
               ['hello_world', 'irt_futex', 'thread', 'float',
                'malloc_realloc_calloc_free', 'dup', 'cond_timedwait',
                'syscall', 'getpid']]
    else:
      # TODO(mseaborn): Use the same test list as on Linux when the threading
      # tests pass for Mac.
      tests = ['run_hello_world_test_irt']
    with Step('unsandboxed_tests', status, halt_on_fail=False):
      SCons(context, parallel=True, mode=irt_mode,
            args=flags_run + ['pnacl_unsandboxed=1'] + tests)


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
