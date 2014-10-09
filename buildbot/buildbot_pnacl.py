#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

from buildbot_lib import (
    BuildContext, BuildStatus, Command, ParseStandardCommandLine,
    RemoveSconsBuildDirectories, RunBuild, SetupLinuxEnvironment,
    SetupMacEnvironment, SetupWindowsEnvironment, SCons, Step )


def RunSconsTests(status, context):
  # Clean out build directories.
  with Step('clobber scons', status):
    RemoveSconsBuildDirectories()

  # Run checkdeps script to vet #includes.
  with Step('checkdeps', status):
    Command(context, cmd=[sys.executable, 'tools/checkdeps/checkdeps.py'])

  # Unlike their arm counterparts we do not run trusted tests on x86 bots.
  # Trusted tests get plenty of coverage by other bots, e.g. nacl-gcc bots.
  # We make the assumption here that there are no "exotic tests" which
  # are trusted in nature but are somehow depedent on the untrusted TC.
  flags_build = ['skip_trusted_tests=1', 'do_not_run_tests=1']
  flags_run = ['skip_trusted_tests=1']
  smoke_tests = ['small_tests', 'medium_tests']

  arch = context['default_scons_platform']

  with Step('build_all ' + arch, status):
    SCons(context, parallel=True, args=flags_build)

  # Normal pexe-mode tests
  with Step('smoke_tests ' + arch, status, halt_on_fail=False):
    SCons(context, parallel=True, args=flags_run + smoke_tests)
  # Large tests cannot be run in parallel
  with Step('large_tests ' + arch, status, halt_on_fail=False):
    SCons(context, parallel=False, args=flags_run + ['large_tests'])

  # non-pexe-mode tests. Build everything to make sure it all builds in nonpexe
  # mode, but just run the nonpexe_tests
  with Step('build_nonpexe ' + arch, status):
    SCons(context, parallel=True, args=flags_build + ['pnacl_generate_pexe=0'])
  with Step('nonpexe_tests ' + arch, status, halt_on_fail=False):
    SCons(context, parallel=True,
          args=flags_run + ['pnacl_generate_pexe=0', 'nonpexe_tests'])

  irt_mode = context['default_scons_mode'] + ['nacl_irt_test']
  smoke_tests_irt = ['small_tests_irt', 'medium_tests_irt']
  # Run some tests with the IRT
  with Step('smoke_tests_irt ' + arch, status, halt_on_fail=False):
    SCons(context, parallel=True, mode=irt_mode,
          args=flags_run + smoke_tests_irt)

  # Test sandboxed translation
  if not context.Windows() and not context.Mac():
    # TODO(dschuff): The standalone sandboxed translator driver does not have
    # the batch script wrappers, so it can't run on Windows. Either add them to
    # the translator package or make SCons use the pnacl_newlib drivers except
    # on the ARM bots where we don't have the pnacl_newlib drivers.
    # The mac standalone sandboxed translator is flaky.
    # https://code.google.com/p/nativeclient/issues/detail?id=3856

    if arch == 'arm':
      # The ARM sandboxed translator is flaky under qemu, so run a very small
      # set of tests there.
      sbtc_tests = ['run_hello_world_test_irt']
    else:
      sbtc_tests = ['toolchain_tests_irt', 'large_code']

    with Step('sandboxed_translator_tests ' + arch, status,
              halt_on_fail=False):
      SCons(context, parallel=True, mode=irt_mode,
            args=flags_run + ['use_sandboxed_translator=1'] + sbtc_tests)
    with Step('sandboxed_translator_fast_tests ' + arch, status,
              halt_on_fail=False):
      SCons(context, parallel=True, mode=irt_mode,
            args=flags_run + ['use_sandboxed_translator=1',
                              'translate_fast=1'] + sbtc_tests)

  # Test Non-SFI Mode.
  # The only architectures that the PNaCl toolchain supports Non-SFI
  # versions of are currently x86-32 and ARM.
  # The x86-64 toolchain bot currently also runs these tests from
  # buildbot_pnacl.sh
  if context.Linux() and (arch == 'x86-32' or arch == 'arm'):
    with Step('nonsfi_tests ' + arch, status, halt_on_fail=False):
      SCons(context, parallel=True, mode=irt_mode,
            args=flags_run +
                ['nonsfi_nacl=1',
                 'nonsfi_tests',
                 'nonsfi_tests_irt'])

    # Test nonsfi_loader linked against host's libc.
    with Step('nonsfi_tests_host_libc ' + arch, status, halt_on_fail=False):
      # Using skip_nonstable_bitcode=1 here disables the tests for
      # zero-cost C++ exception handling, which don't pass for Non-SFI
      # mode yet because we don't build libgcc_eh for Non-SFI mode.
      SCons(context, parallel=True, mode=irt_mode,
            args=flags_run +
                ['nonsfi_nacl=1', 'use_newlib_nonsfi_loader=0',
                 'nonsfi_tests_irt',
                 'toolchain_tests_irt', 'skip_nonstable_bitcode=1'])

  # Test unsandboxed mode.
  if (context.Linux() or context.Mac()) and arch == 'x86-32':
    if context.Linux():
      tests = ['run_' + test + '_test_irt' for test in
               ['hello_world', 'irt_futex', 'thread', 'float',
                'malloc_realloc_calloc_free', 'dup', 'cond_timedwait',
                'getpid']]
    else:
      # TODO(mseaborn): Use the same test list as on Linux when the threading
      # tests pass for Mac.
      tests = ['run_hello_world_test_irt']
    with Step('unsandboxed_tests ' + arch, status, halt_on_fail=False):
      SCons(context, parallel=True, mode=irt_mode,
            args=flags_run + ['pnacl_unsandboxed=1'] + tests)

  # Test MinSFI.
  if not context.Windows() and (arch == 'x86-32' or arch == 'x86-64'):
    with Step('minsfi_tests ' + arch, status, halt_on_fail=False):
      SCons(context, parallel=True,
            args=flags_run + ['minsfi=1', 'minsfi_tests'])

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

  RunBuild(RunSconsTests, status)

if __name__ == '__main__':
  Main()
