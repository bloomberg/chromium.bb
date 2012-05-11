#! -*- python -*-
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import atexit
import glob
import os
import platform
import shutil
import stat
import subprocess
import sys
import zlib
sys.path.append("./common")
sys.path.append('../third_party')
# BUG=http://code.google.com/p/nativeclient/issues/detail?id=135
# simplejson is not available in CrOS chroot due to gclient issues
try:
  import json
except Exception:
  import simplejson as json

from SCons.Errors import UserError
from SCons.Script import GetBuildFailures

import SCons.Warnings
import SCons.Util

SCons.Warnings.warningAsException()

sys.path.append("tools")
import command_tester
import test_lib

# NOTE: Underlay for  src/third_party_mod/gtest
# TODO: try to eliminate this hack
Dir('src/third_party_mod/gtest').addRepository(
    Dir('#/../testing/gtest'))

# Underlay things migrating to ppapi repo.
Dir('..').addRepository(Dir('#/../ppapi'))

# Load a config file from the Chrome repo. This allows scons files to be added
# and removed in Chrome without requiring a DEPS roll.
ppapi_scons_files = {}
execfile(
    File('#/../ppapi/native_client/ppapi_scons_files.py').abspath,
    {}, # Globals
    ppapi_scons_files)

# Append a list of files to another, filtering out the files that already exist.
# Filtering helps migrate declarations between repos by preventing redundant
# declarations from causing an error.
def ExtendFileList(existing, additional):
  # Avoid quadratic behavior by using a set.
  combined = set()
  for file_name in existing + additional:
    if file_name in combined:
      print 'WARNING: two references to file %s in the build.' % file_name
    combined.add(file_name)
  return sorted(combined)

# turning garbage collection off reduces startup time by 10%
import gc
gc.disable()
# ----------------------------------------------------------
# REPORT
# ----------------------------------------------------------
CMD_COUNTER = {}
ENV_COUNTER = {}
def PrintFinalReport():
  """This function is run just before scons exits and dumps various reports.
  """
  # Note, these global declarations are not strictly necessary
  global pre_base_env
  global CMD_COUNTER
  global ENV_COUNTER

  if pre_base_env.Bit('target_stats'):
    print
    print '*' * 70
    print 'COMMAND EXECUTION REPORT'
    print '*' * 70
    for k in sorted(CMD_COUNTER.keys()):
      print "%4d %s" % (CMD_COUNTER[k], k)

    print
    print '*' * 70
    print 'ENVIRONMENT USAGE REPORT'
    print '*' * 70
    for k in sorted(ENV_COUNTER.keys()):
      print "%4d  %s" % (ENV_COUNTER[k], k)

  failures = GetBuildFailures()
  if not failures:
    return

  print
  print '*' * 70
  print 'ERROR REPORT: %d failures' % len(failures)
  print '*' * 70
  print
  for f in failures:
    test_name = GetTestName(f.node)
    raw_name = str(f.node.path)
    # If this wasn't a test, "GetTestName" will return raw_name.
    if test_name != raw_name:
      test_name = '%s (%s)' % (test_name, raw_name)
    print "%s failed: %s\n" % (test_name, f.errstr)

atexit.register(PrintFinalReport)


def VerboseConfigInfo(env):
  "Should we print verbose config information useful for bug reports"
  if '--help' in sys.argv: return False
  if env.Bit('prebuilt') or env.Bit('built_elsewhere'): return False
  return env.Bit('sysinfo')

# ----------------------------------------------------------
# SANITY CHECKS
# ----------------------------------------------------------

# NOTE BitFromArgument(...) implicitly defines additional ACCEPTABLE_ARGUMENTS.
ACCEPTABLE_ARGUMENTS = set([
    # TODO: add comments what these mean
    # TODO: check which ones are obsolete
    ####  ASCII SORTED ####
    # Use a destination directory other than the default "scons-out".
    'DESTINATION_ROOT',
    'DOXYGEN',
    'MODE',
    'SILENT',
    # set build platform
    'buildplatform',
    # Location to download Chromium binaries to and/or read them from.
    'chrome_binaries_dir',
    # used for chrome_browser_tests: path to the browser
    'chrome_browser_path',
    # A comma-separated list of test names to disable by excluding the
    # tests from a test suite.  For example, 'small_tests
    # disable_tests=run_hello_world_test' will run small_tests without
    # including hello_world_test.  Note that if a test listed here
    # does not exist you will not get an error or a warning.
    'disable_tests',
    # used for chrome_browser_tests: path to a pre-built browser plugin.
    'force_ppapi_plugin',
    # force emulator use by tests
    'force_emulator',
    # force sel_ldr use by tests
    'force_sel_ldr',
    # force irt image used by tests
    'force_irt',
    # colon-separated list of linker flags, e.g. "-lfoo:-Wl,-u,bar".
    'nacl_linkflags',
    # colon-separated list of pnacl bcld flags, e.g. "-lfoo:-Wl,-u,bar".
    # Not using nacl_linkflags since that gets clobbered in some tests.
    'pnacl_bcldflags',
    'naclsdk_mode',
    # set both targetplatform and buildplatform
    'platform',
    # Run tests under this tool (e.g. valgrind, tsan, strace, etc).
    # If the tool has options, pass them after comma: 'tool,--opt1,--opt2'.
    # NB: no way to use tools the names or the args of
    # which contains a comma.
    'run_under',
    # More args for the tool.
    'run_under_extra_args',
    # Multiply timeout values by this number.
    'scale_timeout',
    # Run browser tests under this tool. See
    # tools/browser_tester/browsertester/browserlauncher.py for tool names.
    'browser_test_tool',
    # set target platform
    'targetplatform',
    # activates buildbot-specific presets
    'buildbot',
    # Where to install header files for public consumption.
    'includedir',
    # Where to install libraries for public consumption.
    'libdir',
    # Where to install trusted-code binaries for public (SDK) consumption.
    'bindir',
  ])


# Overly general to provide compatibility with existing build bots, etc.
# In the future it might be worth restricting the values that are accepted.
_TRUE_STRINGS = set(['1', 'true', 'yes'])
_FALSE_STRINGS = set(['0', 'false', 'no'])


# Converts a string representing a Boolean value, of some sort, into an actual
# Boolean value. Python's built in type coercion does not work because
# bool('False') == True
def StringValueToBoolean(value):
  # ExpandArguments may stick non-string values in ARGUMENTS. Be accommodating.
  if isinstance(value, bool):
    return value

  if not isinstance(value, basestring):
    raise Exception("Expecting a string but got a %s" % repr(type(value)))

  if value.lower() in _TRUE_STRINGS:
    return True
  elif value.lower() in _FALSE_STRINGS:
    return False
  else:
    raise Exception("Cannot convert '%s' to a Boolean value" % value)


def GetBinaryArgumentValue(arg_name, default):
  if not isinstance(default, bool):
    raise Exception("Default value for '%s' must be a Boolean" % arg_name)
  if arg_name not in ARGUMENTS:
    return default
  return StringValueToBoolean(ARGUMENTS[arg_name])


# name is the name of the bit
# arg_name is the name of the command-line argument, if it differs from the bit
def BitFromArgument(env, name, default, desc, arg_name=None):
  # In most cases the bit name matches the argument name
  if arg_name is None:
    arg_name = name

  DeclareBit(name, desc)
  assert arg_name not in ACCEPTABLE_ARGUMENTS, repr(arg_name)
  ACCEPTABLE_ARGUMENTS.add(arg_name)

  if GetBinaryArgumentValue(arg_name, default):
    env.SetBits(name)
  else:
    env.ClearBits(name)


# SetUpArgumentBits declares binary command-line arguments and converts them to
# bits. For example, one of the existing declarations would result in the
# argument "bitcode=1" causing env.Bit('bitcode') to evaluate to true.
# NOTE Command-line arguments are a SCons-ism that is separate from
# command-line options.  Options are prefixed by "-" or "--" whereas arguments
# are not.  The function SetBitFromOption can be used for options.
# NOTE This function must be called before the bits are used
# NOTE This function must be called after all modifications of ARGUMENTS have
# been performed. See: ExpandArguments
def SetUpArgumentBits(env):
  BitFromArgument(env, 'bitcode', default=False,
    desc='We are building bitcode')

  BitFromArgument(env, 'translate_fast', default=False,
    desc='When using pnacl TC (bitcode=1) use accelerated translation step')

  BitFromArgument(env, 'built_elsewhere', default=False,
    desc='The programs have already been built by another system')

  BitFromArgument(env, 'nacl_pic', default=False,
    desc='generate position indepent code for (P)NaCl modules')

  BitFromArgument(env, 'nacl_static_link', default=not env.Bit('nacl_glibc'),
    desc='Whether to use static linking instead of dynamic linking '
      'for building NaCl executables during tests. '
      'For nacl-newlib, the default is 1 (static linking). '
      'For nacl-glibc, the default is 0 (dynamic linking).')

  BitFromArgument(env, 'nacl_disable_shared', default=not env.Bit('nacl_glibc'),
    desc='Do not build shared versions of libraries. '
      'For nacl-newlib, the default is 1 (static libraries only). '
      'For nacl-glibc, the default is 0 (both static and shared libraries).')

  # Defaults on when --verbose is specified.
  # --verbose sets 'brief_comstr' to False, so this looks a little strange
  BitFromArgument(env, 'target_stats', default=not GetOption('brief_comstr'),
    desc='Collect and display information about which commands are executed '
      'during the build process')

  BitFromArgument(env, 'werror', default=True,
    desc='Treat warnings as errors (-Werror)')

  BitFromArgument(env, 'disable_nosys_linker_warnings', default=False,
    desc='Disable warning mechanism in src/untrusted/nosys/warning.h')

  BitFromArgument(env, 'naclsdk_validate', default=True,
    desc='Verify the presence of the SDK')

  BitFromArgument(env, 'running_on_valgrind', default=False,
    desc='Compile and test using valgrind')

  # Linking in -lcrypto makes sel_ldr binaries that depend on a
  # particular shared library version being available, which makes
  # the SDK binaries less portable among Linux variants.  With this
  # switch off, standalone sel_ldr relies on /dev/urandom as the
  # Chromium incarnation will.
  BitFromArgument(env, 'use_libcrypto', default=False,
    desc='Use libcrypto')

  BitFromArgument(env, 'enable_tmpfs_redirect_var', default=False,
    desc='Allow redirecting tmpfs location for shared memory '
         '(by default, /dev/shm is used)')

  BitFromArgument(env, 'pp', default=False,
    desc='Enable pretty printing')

  # By default SCons does not use the system's environment variables when
  # executing commands, to help isolate the build process.
  BitFromArgument(env, 'use_environ', arg_name='USE_ENVIRON',
    default=False, desc='Expose existing environment variables to the build')

  # Defaults on when --verbose is specified
  # --verbose sets 'brief_comstr' to False, so this looks a little strange
  BitFromArgument(env, 'sysinfo', default=not GetOption('brief_comstr'),
    desc='Print verbose system information')

  BitFromArgument(env, 'disable_dynamic_plugin_loading', default=False,
    desc='Use the version of NaCl that is built into Chromium.  '
      'Disables use of --register-pepper-plugins, so that the '
      'externally-built NaCl plugin and sel_ldr are not used.')

  BitFromArgument(env, 'override_chrome_irt', default=False,
    desc='When using the version of NaCl that is built into Chromium, '
      'this prevents use of the IRT that is built into Chromium, and uses '
      'a newly-built IRT library.')

  BitFromArgument(env, 'disable_flaky_tests', default=False,
    desc='Do not run potentially flaky tests - used on Chrome bots')

  BitFromArgument(env, 'use_sandboxed_translator', default=False,
    desc='use pnacl sandboxed translator for linking (not available for arm)')

  BitFromArgument(env, 'pnacl_generate_pexe', default=env.Bit('bitcode'),
    desc='use pnacl to generate pexes and translate in a separate step')

  BitFromArgument(env, 'translate_in_build_step', default=True,
    desc='Run translation during build phase (e.g. if do_not_run_tests=1)')

  # This only controls whether the sandboxed translator is itself dynamically
  # linked, not whether it generates dynamic nexes (or links against glibc)
  #  --nacl_glibc should still control that.
  # it only has effect if use_sandboxed_translator is also true
  # TODO(dschuff) this can go away when we are no longer required to explicitly
  # invoke runnable-ld.so
  BitFromArgument(env, 'sandboxed_translator_is_dynamic', default=False,
    desc='used dynamically-linked sandboxed translator')

  BitFromArgument(env, 'browser_headless', default=False,
    desc='Where possible, set up a dummy display to run the browser on '
      'when running browser tests.  On Linux, this runs the browser through '
      'xvfb-run.  This Scons does not need to be run with an X11 display '
      'and we do not open a browser window on the user\'s desktop.  '
      'Unfortunately there is no equivalent on Mac OS X.')

  BitFromArgument(env, 'disable_crash_dialog', default=True,
    desc='Disable Windows\' crash dialog box, which Windows pops up when a '
      'process exits with an unhandled fault.  Windows enables this by '
      'default for processes launched from the command line or from the '
      'GUI.  Our default is to disable it, because the dialog turns crashes '
      'into hangs on Buildbot, and our test suite includes various crash '
      'tests.')

  BitFromArgument(env, 'do_not_run_tests', default=False,
    desc='Prevents tests from running.  This lets SCons build the files needed '
      'to run the specified test(s) without actually running them.  This '
      'argument is a counterpart to built_elsewhere.')

  #########################################################################
  # EXPERIMENTAL
  # This is for generating a testing library for use within private test
  # enuminsts, where we want to compare and test different validators.
  #
  BitFromArgument(env, 'ncval_testing', default=False,
    desc='EXPERIMENTAL: Compile validator code for testing within enuminsts')

  # PNaCl sanity checks
  if ((env.Bit('pnacl_generate_pexe') or env.Bit('use_sandboxed_translator'))
      and not env.Bit('bitcode')):
    raise ValueError("pnacl_generate_pexe and use_sandboxed_translator"
                        "don't make sense without bitcode")


def CheckArguments():
  for key in ARGUMENTS:
    if key not in ACCEPTABLE_ARGUMENTS:
      print 'ERROR'
      print 'ERROR bad argument: %s' % key
      print 'ERROR'
      sys.exit(-1)


# Sets a command line argument. Dies if an argument with this name is already
# defined.
def SetArgument(key, value):
  print '    %s=%s' % (key, str(value))
  if key in ARGUMENTS:
    print 'ERROR: %s redefined' % (key, )
    sys.exit(-1)
  else:
    ARGUMENTS[key] = value

# Expands "macro" command line arguments.
def ExpandArguments():
  if ARGUMENTS.get('buildbot') == 'memcheck':
    print 'buildbot=memcheck expands to the following arguments:'
    SetArgument('run_under',
                'src/third_party/valgrind/memcheck.sh,' +
                '--error-exitcode=1')
    SetArgument('scale_timeout', 20)
    SetArgument('running_on_valgrind', True)
  elif ARGUMENTS.get('buildbot') == 'tsan':
    print 'buildbot=tsan expands to the following arguments:'
    SetArgument('run_under',
                'src/third_party/valgrind/tsan.sh,' +
                '--nacl-untrusted,--error-exitcode=1,' +
                '--suppressions=src/third_party/valgrind/tests.supp')
    SetArgument('scale_timeout', 20)
    SetArgument('running_on_valgrind', True)
  elif ARGUMENTS.get('buildbot') == 'tsan-trusted':
    print 'buildbot=tsan-trusted expands to the following arguments:'
    SetArgument('run_under',
                'src/third_party/valgrind/tsan.sh,' +
                '--error-exitcode=1,' +
                '--suppressions=src/third_party/valgrind/tests.supp')
    SetArgument('scale_timeout', 20)
    SetArgument('running_on_valgrind', True)
  elif ARGUMENTS.get('buildbot') == 'memcheck-browser-tests':
    print 'buildbot=memcheck-browser-tests expands to the following arguments:'
    SetArgument('browser_test_tool', 'memcheck')
    SetArgument('scale_timeout', 20)
    SetArgument('running_on_valgrind', True)
  elif ARGUMENTS.get('buildbot') == 'tsan-browser-tests':
    print 'buildbot=tsan-browser-tests expands to the following arguments:'
    SetArgument('browser_test_tool', 'tsan')
    SetArgument('scale_timeout', 20)
    SetArgument('running_on_valgrind', True)
  elif ARGUMENTS.get('buildbot'):
    print 'ERROR: unexpected argument buildbot="%s"' % (
        ARGUMENTS.get('buildbot'), )
    sys.exit(-1)

#-----------------------------------------------------------
ExpandArguments()

# ----------------------------------------------------------
environment_list = []

# ----------------------------------------------------------
# Base environment for both nacl and non-nacl variants.
kwargs = {}
if ARGUMENTS.get('DESTINATION_ROOT') is not None:
  kwargs['DESTINATION_ROOT'] = ARGUMENTS.get('DESTINATION_ROOT')
pre_base_env = Environment(
    tools = ['component_setup'],
    # SOURCE_ROOT is one leave above the native_client directory.
    SOURCE_ROOT = Dir('#/..').abspath,
    # Publish dlls as final products (to staging).
    COMPONENT_LIBRARY_PUBLISH = True,

    # Use workaround in special scons version.
    LIBS_STRICT = True,
    LIBS_DO_SUBST = True,

    # Select where to find coverage tools.
    COVERAGE_MCOV = '../third_party/lcov/bin/mcov',
    COVERAGE_GENHTML = '../third_party/lcov/bin/genhtml',
    **kwargs
)

# ----------------------------------------------------------
# CLANG
# ----------------------------------------------------------
DeclareBit('clang', 'Use clang to build trusted code')
pre_base_env.SetBitFromOption('clang', False)

DeclareBit('asan',
           'Use AddressSanitizer to build trusted code (implies --clang)')
pre_base_env.SetBitFromOption('asan', False)
if pre_base_env.Bit('asan'):
  pre_base_env.SetBits('clang')

# ----------------------------------------------------------
# CODE COVERAGE
# ----------------------------------------------------------
DeclareBit('coverage_enabled', 'The build should be instrumented to generate'
           'coverage information')

# If the environment variable BUILDBOT_BUILDERNAME is set, we can determine
# if we are running in a VM by the lack of a '-bare-' (aka bare metal) in the
# bot name.  Otherwise if the builder name is not set, then assume real HW.
DeclareBit('running_on_vm', 'Returns true when environment is running in a VM')
builder = os.environ.get('BUILDBOT_BUILDERNAME')
if builder and builder.find('-bare-') == -1:
  pre_base_env.SetBits('running_on_vm')
else:
  pre_base_env.ClearBits('running_on_vm')

DeclareBit('nacl_glibc', 'Use nacl-glibc for building untrusted code')
pre_base_env.SetBitFromOption('nacl_glibc', False)

# This function should be called ASAP after the environment is created, but
# after ExpandArguments.
SetUpArgumentBits(pre_base_env)

def DisableCrashDialog():
  if sys.platform == 'win32':
    import win32api
    import win32con
    # The double call is to preserve existing flags, as discussed at
    # http://blogs.msdn.com/oldnewthing/archive/2004/07/27/198410.aspx
    new_flags = win32con.SEM_NOGPFAULTERRORBOX
    existing_flags = win32api.SetErrorMode(new_flags)
    win32api.SetErrorMode(existing_flags | new_flags)

if pre_base_env.Bit('disable_crash_dialog'):
  DisableCrashDialog()

# Scons normally wants to scrub the environment.  However, sometimes
# we want to allow PATH and other variables through so that Scons
# scripts can find nacl-gcc without needing a Scons-specific argument.
if pre_base_env.Bit('use_environ'):
  pre_base_env['ENV'] = os.environ.copy()

# We want to pull CYGWIN setup in our environment or at least set flag
# nodosfilewarning. It does not do anything when CYGWIN is not involved
# so let's do it in all cases.
pre_base_env['ENV']['CYGWIN'] = os.environ.get('CYGWIN', 'nodosfilewarning')

if pre_base_env.Bit('werror'):
  werror_flags = ['-Werror']
else:
  werror_flags = []

# Allow variadic macros
werror_flags = werror_flags + ['-Wno-variadic-macros']

# Clang Bug:
# array-bounds detection gives false positive on valid code in GLibC.
# BUG= http://llvm.org/bugs/show_bug.cgi?id=11536
if pre_base_env.Bit('bitcode') and pre_base_env.Bit('nacl_glibc'):
  werror_flags += ['-Wno-array-bounds']

if pre_base_env.Bit('clang'):
  # Allow 'default' label in switch even when all enumeration cases
  # have been covered.
  werror_flags += ['-Wno-covered-switch-default']
  # Allow C++11 extensions (for "override")
  werror_flags += ['-Wno-c++11-extensions']


# ----------------------------------------------------------
# Method to make sure -pedantic, etc, are not stripped from the
# default env, since occasionally an engineer will be tempted down the
# dark -- but wide and well-trodden -- path of expediency and stray
# from the path of correctness.

def EnsureRequiredBuildWarnings(env):
  if env.Bit('linux') or env.Bit('mac'):
    required_env_flags = set(['-pedantic', '-Wall'] + werror_flags)
    ccflags = set(env.get('CCFLAGS'))

    if not required_env_flags.issubset(ccflags):
      raise UserError('required build flags missing: '
                      + ' '.join(required_env_flags.difference(ccflags)))
  else:
    # windows get a pass for now
    pass

pre_base_env.AddMethod(EnsureRequiredBuildWarnings)

# ----------------------------------------------------------
# Method to add target suffix to name.
def NaClTargetArchSuffix(env, name):
  return name + '_' + env['TARGET_FULLARCH'].replace('-', '_')

pre_base_env.AddMethod(NaClTargetArchSuffix)

# ----------------------------------------------------------
# Generic Test Wrapper

# ----------------------------------------------------------
# Add list of Flaky or Bad tests to skip per platform.  A
# platform is defined as build type
# <BUILD_TYPE>-<SUBARCH>
bad_build_lists = {
    'arm': [],
}

# This is a list of tests that do not yet pass when using nacl-glibc.
# TODO(mseaborn): Enable more of these tests!
nacl_glibc_skiplist = set([
    # These tests assume they can allocate address ranges from the
    # dynamic code area themselves.  However, this does not work when
    # ld.so assumes it can do the same.  To fix this, ld.so will need
    # an interface, like mmap() or brk(), for reserving space in the
    # dynamic code area.
    'run_dynamic_load_test',
    'run_write_to_dyncode_test',
    'run_dyncode_debug_mode_test',
    'run_dyncode_demand_alloc_test',
    'run_dyncode_disabled_test',
    'run_dynamic_modify_test',
    # This uses a canned binary that is compiled w/ newlib.  A
    # glibc version might be useful.
    'run_fuzz_nullptr_test',
    # Struct layouts differ.
    'run_abi_test',
    # Syscall wrappers not implemented yet.
    'run_sysbasic_test',
    'run_sysbrk_test',
    # Fails because clock() is not hooked up.
    'run_timefuncs_test',
    # Needs further investigation.
    'sdk_minimal_test',
    # TODO(elijahtaylor) add apropriate syscall hooks for glibc
    'run_gc_instrumentation_test',
    # run_srpc_sysv_shm_test fails because:
    # 1) it uses fstat(), while we only have an fstat64() wrapper;
    # 2) the test needs an explicit fflush(stdout) call because the
    # process is killed without exit() being called.
    'run_srpc_sysv_shm_test',
    # This test fails with nacl-glibc: glibc reports an internal
    # sanity check failure in free().
    # TODO(robertm): This needs further investigation.
    'run_ppapi_event_test',
    'run_srpc_ro_file_test',
    'run_ppapi_geturl_valid_test',
    'run_ppapi_geturl_invalid_test',
    # http://code.google.com/p/chromium/issues/detail?id=108131
    # we would need to list all of the glibc components as
    # web accessible resources in the extensions's manifest.json,
    # not just the nexe and nmf file.
    'run_ppapi_extension_mime_handler_browser_test',

    # This test need more investigation.
    'run_syscall_test',
    # GetPid is no longer supplied by IRT so does not work in GLibC.
    'run_getpid_test',
    'run_getpid_disabled_test',
    ])


# If a test is not in one of these suites, it will probally not be run on a
# regular basis.  These are the suites that will be run by the try bot or that
# a large number of users may run by hand.
MAJOR_TEST_SUITES = set([
  'small_tests',
  'medium_tests',
  'large_tests',
  # Tests using the pepper plugin, only run with chrome
  # TODO(ncbray): migrate pepper_browser_tests to chrome_browser_tests
  'pepper_browser_tests',
  # Lightweight browser tests
  'chrome_browser_tests',
  # Tests written using Chrome's PyAuto test jig
  'pyauto_tests',
  'huge_tests',
  'memcheck_bot_tests',
  'tsan_bot_tests',
  # Special testing environment for testing comparing x86 validators.
  'ncval_testing',
])

# These are the test suites we know exist, but aren't run on a regular basis.
# These test suites are essentially shortcuts that run a specific subset of the
# test cases.
ACCEPTABLE_TEST_SUITES = set([
  'barebones_tests',
  'dynamic_load_tests',
  'exception_tests',
  'exit_status_tests',
  'mmap_race_tests',
  'nonpexe_tests',
  'performance_tests',
  'pnacl_abi_tests',
  'sel_ldr_sled_tests',
  'sel_ldr_tests',
  'toolchain_tests',
  'validator_modeling',
  'validator_tests',
  # Special testing of the decoder for the ARM validator.
  'arm_decoder_tests',
])

# Under --mode=nacl_irt_test we build variants of numerous tests normally
# built under --mode=nacl.  The test names and suite names for these
# variants are set (in IrtTestAddNodeToTestSuite, below) by appending _irt
# to the names used for the --mode=nacl version of the same tests.
MAJOR_TEST_SUITES |= set([name + '_irt'
                          for name in MAJOR_TEST_SUITES])
ACCEPTABLE_TEST_SUITES |= set([name + '_irt'
                               for name in ACCEPTABLE_TEST_SUITES])

# The major test suites are also acceptable names.  Suite names are checked
# against this set in order to catch typos.
ACCEPTABLE_TEST_SUITES.update(MAJOR_TEST_SUITES)


def ValidateTestSuiteNames(suite_name, node_name):
  if node_name is None:
    node_name = '<unknown>'

  # Prevent a silent failiure - strings are iterable!
  if not isinstance(suite_name, (list, tuple)):
    raise Exception("Test suites for %s should be specified as a list, "
      "not as a %s: %s" % (node_name, type(suite_name).__name__,
      repr(suite_name)))

  if not suite_name:
    raise Exception("No test suites are specified for %s. Set the 'broken' "
      "parameter on AddNodeToTestSuite in the cases where there's a known "
      "issue and you don't want the test to run" % (node_name,))

  # Make sure each test is in at least one test suite we know will run
  major_suites = set(suite_name).intersection(MAJOR_TEST_SUITES)
  if not major_suites:
    raise Exception("None of the test suites %s for %s are run on a "
    "regular basis" % (repr(suite_name), node_name))

  # Make sure a wierd test suite hasn't been inadvertantly specified
  for s in suite_name:
    if s not in ACCEPTABLE_TEST_SUITES:
      raise Exception("\"%s\" is not a known test suite. Either this is "
      "a typo for %s, or it should be added to ACCEPTABLE_TEST_SUITES in "
      "SConstruct" % (s, node_name))

BROKEN_TEST_COUNT = 0


def GetPlatformString(env):
  build = env['BUILD_TYPE']

  # If we are testing 'NACL' we really need the trusted info
  if build=='nacl' and 'TRUSTED_ENV' in env:
    trusted_env = env['TRUSTED_ENV']
    build = trusted_env['BUILD_TYPE']
    subarch = trusted_env['BUILD_SUBARCH']
  else:
    subarch = env['BUILD_SUBARCH']

  # Build the test platform string
  return build + '-' + subarch


tests_to_disable = set([
    # these sel_universal tests are broken and it's won't fix
    # http://code.google.com/p/nativeclient/issues/detail?id=2390
    'run_ppapi_simple_audio_test',
    'run_ppapi_simple_graphics2d_test',
    'run_ppapi_emu_file_test',
])

tests_to_disable_qemu = set([
    # These tests do not work under QEMU but do work on hardware. They should
    # not be marked as 'broken' because then they would not get built as part
    # of a test target (e.g. small_tests) with do_not_run_tests=1. They
    # should be skipped instead, so that they build but do not run.
    # TODO(dschuff) some of these tests appear to work with the new QMEU.
    # find out which
    # http://code.google.com/p/nativeclient/issues/detail?id=2437
    'run_atomic_ops_test',
    'run_atomic_ops_nexe_test',
    'run_egyptian_cotton_test',
    'run_gio_shm_unbounded_test',
    'run_many_threads_sequential_test',
    'run_mmap_atomicity_test',
    'run_mmap_test',
    # http://code.google.com/p/nativeclient/issues/detail?id=2142
    'run_nacl_semaphore_test',
    'run_nacl_tls_unittest',
    # The next 2 tests seem flaky on QEMU
    'run_srpc_manifest_file_test',
    'run_srpc_message_untrusted_test',
    'run_thread_stack_alloc_test',
    'run_thread_test',
    'run_tool_tests',
    'run_trusted_mmap_test',
])

if ARGUMENTS.get('disable_tests', '') != '':
  tests_to_disable.update(ARGUMENTS['disable_tests'].split(','))

def ShouldSkipTest(env, node_name):
  # There are no known-to-fail tests any more, but this code is left
  # in so that if/when we port to a new architecture or add a test
  # that is known to fail on some platform(s), we can continue to have
  # a central location to disable tests from running.  NB: tests that
  # don't *build* on some platforms need to be omitted in another way.

  if node_name in tests_to_disable:
    return True

  if env.UsingEmulator() and node_name in tests_to_disable_qemu:
    return True

  # Retrieve list of tests to skip on this platform
  skiplist = bad_build_lists.get(GetPlatformString(env), [])
  if node_name in skiplist:
    return True

  if env.Bit('nacl_glibc') and node_name in nacl_glibc_skiplist:
    return True

  return False


def AddNodeToTestSuite(env, node, suite_name, node_name=None, is_broken=False,
                       is_flaky=False):
  global BROKEN_TEST_COUNT

  # CommandTest can return an empty list when it silently discards a test
  if not node:
    return

  ValidateTestSuiteNames(suite_name, node_name)

  AlwaysBuild(node)

  if node_name:
    display_name = node_name
  else:
    display_name = '<no name>'

  if is_broken or is_flaky and env.Bit('disable_flaky_tests'):
    # Only print if --verbose is specified
    if not GetOption('brief_comstr'):
      print '*** BROKEN ', display_name
    BROKEN_TEST_COUNT += 1
    env.Alias('broken_tests', node)
  elif ShouldSkipTest(env, node_name) and not env.Bit('do_not_run_tests'):
    print '*** SKIPPING ', GetPlatformString(env), ':', display_name
    env.Alias('broken_tests', node)
  else:
    env.Alias('all_tests', node)

    for s in suite_name:
      env.Alias(s, node)

  if node_name:
    env.ComponentTestOutput(node_name, node)
    test_name = node_name
  else:
    # This is rather shady, but the tests need a name without dots so they match
    # what gtest does.
    # TODO(ncbray) node_name should not be optional.
    test_name = os.path.basename(str(node[0].path))
    if test_name.endswith('.out'):
      test_name = test_name[:-4]
    test_name = test_name.replace('.', '_')
  SetTestName(node, test_name)


pre_base_env.AddMethod(AddNodeToTestSuite)

# ----------------------------------------------------------
# Convenient testing aliases
# NOTE: work around for scons non-determinism in the following two lines
Alias('sel_ldr_sled_tests', [])

Alias('small_tests', [])
Alias('medium_tests', [])
Alias('large_tests', [])
Alias('pepper_browser_tests', [])
Alias('chrome_browser_tests', [])
Alias('pyauto_tests', [])

Alias('unit_tests', 'small_tests')
Alias('smoke_tests', ['small_tests', 'medium_tests'])

if pre_base_env.Bit('nacl_glibc'):
  Alias('memcheck_bot_tests', ['small_tests'])
  Alias('tsan_bot_tests', ['small_tests'])
else:
  Alias('memcheck_bot_tests', ['small_tests', 'medium_tests', 'large_tests'])
  Alias('tsan_bot_tests', [])


# ----------------------------------------------------------
def Banner(text):
  print '=' * 70
  print text
  print '=' * 70

pre_base_env.AddMethod(Banner)

# ----------------------------------------------------------
# PLATFORM LOGIC
# ----------------------------------------------------------
# Define the build and target platforms, and use them to define the path
# for the scons-out directory (aka TARGET_ROOT)
#.
# We have "build" and "target" platforms for the non nacl environments
# which govern service runtime, validator, etc.
#
# "build" means the  platform the code is running on
# "target" means the platform the validator is checking for.
# Typically they are the same but testing it useful to have flexibility.
#
# Various variables in the scons environment are related to this, e.g.
#
# BUILD_ARCH: (arm, x86)
# BUILD_SUBARCH: (32, 64)
#
# The settings can be controlled using scons command line variables:
#
#
# buildplatform=: controls the build platform
# targetplatform=: controls the target platform
# platform=: controls both
#
# This dictionary is used to translate from a platform name to a
# (arch, subarch) pair
AVAILABLE_PLATFORMS = {
    'x86-32'      : { 'arch' : 'x86' , 'subarch' : '32' },
    'x86-64'      : { 'arch' : 'x86' , 'subarch' : '64' },
    'arm'         : { 'arch' : 'arm' , 'subarch' : '32' },
    'arm-thumb2'  : { 'arch' : 'arm' , 'subarch' : '32' }
    }

# Look up the platform name from the command line arguments,
# defaulting to 'platform' if needed.
def GetPlatform(name):
  platform = ARGUMENTS.get(name)
  if platform is None:
    return ARGUMENTS.get('platform', 'x86-32')
  elif ARGUMENTS.get('platform') is None:
    return platform
  else:
    raise Exception('Can\'t specify both %s and %s on the command line'
                    % ('platform', name))


# Decode platform into list [ ARCHITECTURE , EXEC_MODE ].
def DecodePlatform(platform):
  if platform in AVAILABLE_PLATFORMS:
    return AVAILABLE_PLATFORMS[platform]
  raise Exception('Unrecognized platform: %s' % platform)


DeclareBit('build_x86_32', 'Building binaries for the x86-32 architecture',
           exclusive_groups='build_arch')
DeclareBit('build_x86_64', 'Building binaries for the x86-64 architecture',
           exclusive_groups='build_arch')
DeclareBit('build_arm_arm', 'Building binaries for the ARM architecture',
           exclusive_groups='build_arch')
DeclareBit('build_arm_thumb2',
           'Building binaries for the ARM architecture (thumb2 ISA)',
           exclusive_groups='build_arch')
DeclareBit('target_x86_32', 'Tools being built will process x86-32 binaries',
           exclusive_groups='target_arch')
DeclareBit('target_x86_64', 'Tools being built will process x86-36 binaries',
           exclusive_groups='target_arch')
DeclareBit('target_arm_arm', 'Tools being built will process ARM binaries',
           exclusive_groups='target_arch')
DeclareBit('target_arm_thumb2',
           'Tools being built will process ARM binaries (thumb2 ISA)',
           exclusive_groups='target_arch')

# Shorthand for either the 32 or 64 bit version of x86.
DeclareBit('build_x86', 'Building binaries for the x86 architecture')
DeclareBit('target_x86', 'Tools being built will process x86 binaries')

# Shorthand for either arm or thumb2 versions of ARM
DeclareBit('build_arm', 'Building binaries for the arm architecture')
DeclareBit('target_arm', 'Tools being built will process arm binaries')


def MakeArchSpecificEnv():
  env = pre_base_env.Clone()
  BUILD_NAME = GetPlatform('buildplatform')
  env.Replace(BUILD_FULLARCH=BUILD_NAME)
  env.Replace(BUILD_ARCHITECTURE=DecodePlatform(BUILD_NAME)['arch'])
  env.Replace(BUILD_SUBARCH=DecodePlatform(BUILD_NAME)['subarch'])

  TARGET_NAME = GetPlatform('targetplatform')
  env.Replace(TARGET_FULLARCH=TARGET_NAME)
  env.Replace(TARGET_ARCHITECTURE=DecodePlatform(TARGET_NAME)['arch'])
  env.Replace(TARGET_SUBARCH=DecodePlatform(TARGET_NAME)['subarch'])

  # Example: PlatformBit('build', 'x86-32') -> build_x86_32
  def PlatformBit(prefix, platform):
    return "%s_%s" % (prefix, platform.replace('-', '_'))

  env.SetBits(PlatformBit('build', BUILD_NAME))
  env.SetBits(PlatformBit('target', TARGET_NAME))

  if env.Bit('build_x86_32') or env.Bit('build_x86_64'):
    env.SetBits('build_x86')
  if env.Bit('build_arm_arm') or env.Bit('build_arm_thumb2'):
    env.SetBits('build_arm')

  if env.Bit('target_x86_32') or env.Bit('target_x86_64'):
    env.SetBits('target_x86')
  if env.Bit('target_arm_arm') or env.Bit('target_arm_thumb2'):
    env.SetBits('target_arm')

  env.Replace(BUILD_ISA_NAME=GetPlatform('buildplatform'))

  if env.Bit('target_arm') and not env.Bit('bitcode'):
    # This has always been a silent default on ARM.
    env.SetBits('bitcode')

  # Determine where the object files go
  if BUILD_NAME == TARGET_NAME:
    BUILD_TARGET_NAME = TARGET_NAME
  else:
    BUILD_TARGET_NAME = '%s-to-%s' % (BUILD_NAME, TARGET_NAME)
  env.Replace(BUILD_TARGET_NAME=BUILD_TARGET_NAME)
  # This may be changed later; see target_variant_map, below.
  env.Replace(TARGET_VARIANT='')
  env.Replace(TARGET_ROOT=
      '${DESTINATION_ROOT}/${BUILD_TYPE}-${BUILD_TARGET_NAME}${TARGET_VARIANT}')
  return env


# Valgrind
pre_base_env.AddMethod(lambda self: ARGUMENTS.get('running_on_valgrind'),
                       'IsRunningUnderValgrind')

DeclareBit('with_leakcheck', 'Running under Valgrind leak checker')

def RunningUnderLeakCheck():
  run_under = ARGUMENTS.get('run_under')
  if run_under:
    extra_args = ARGUMENTS.get('run_under_extra_args')
    if extra_args:
      run_under += extra_args
    if run_under.find('leak-check=full') > 0:
      return True
  return False

if RunningUnderLeakCheck():
  pre_base_env.SetBits('with_leakcheck')

# This method indicates that the binaries we are building will validate code
# for an architecture different than the one the binaries will run on.
# NOTE Currently (2010/11/17) this is 'x86' vs. 'arm', and  x86-32 vs. x86-64
# is not considered to be a cross-tools build.
def CrossToolsBuild(env):
  return env['BUILD_ARCHITECTURE'] != env['TARGET_ARCHITECTURE']

pre_base_env.AddMethod(CrossToolsBuild, 'CrossToolsBuild')

# ----------------------------------------------------------
def HasSuffix(item, suffix):
  if isinstance(item, str):
    return item.endswith(suffix)
  elif hasattr(item, '__getitem__'):
    return HasSuffix(item[0], suffix)
  else:
    return item.path.endswith(suffix)


def DualLibrary(env, lib_name, *args, **kwargs):
  """Builder to build both .a and _shared.a library in one step.

  Args:
    env: Environment in which we were called.
    lib_name: Library name.
    args: Positional arguments.
    kwargs: Keyword arguments.
  """
  static_objs = [i for i in Flatten(args[0]) if not HasSuffix(i, '.os')]
  shared_objs = [i for i in Flatten(args[0]) if not HasSuffix(i, '.o')]
  # Built static library as ususal.
  env.ComponentLibrary(lib_name, static_objs, **kwargs)
  # For coverage bots, we only want one object file since two versions would
  # write conflicting information to the same .gdca/.gdna files.
  if env.Bit('coverage_enabled'): return
  # Build a static library using -fPIC for the .o's.
  if env.Bit('linux'):
    env_shared = env.Clone(OBJSUFFIX='.os')
    env_shared.Append(CCFLAGS=['-fPIC'])
    # -fPIE overrides -fPIC, and shared libraries should not be linked
    # as executables.
    env_shared.FilterOut(CCFLAGS=['-fPIE'])
    env_shared.ComponentLibrary(lib_name + '_shared', shared_objs, **kwargs)
    # for arm trusted we usually build -static
    env_shared.FilterOut(LINKFLAGS=['-static'])

def DualObject(env, *args, **kwargs):
  """Builder to build both .o and .os in one step.

  Args:
    env: Environment in which we were called.
    args: Positional arguments.
    kwargs: Keyword arguments.
  """
  # Built static library as ususal.
  ret = env.ComponentObject(*args, **kwargs)
  # For coverage bots, we only want one object file since two versions would
  # write conflicting information to the same .gdca/.gdna files.
  if env.Bit('coverage_enabled'): return ret
  # Build a static library using -fPIC for the .o's.
  if env.Bit('linux'):
    env_shared = env.Clone(OBJSUFFIX='.os')
    env_shared.Append(CCFLAGS=['-fPIC'])
    ret += env_shared.ComponentObject(*args, **kwargs)
  return ret


def AddDualLibrary(env):
  env.AddMethod(DualLibrary)
  env.AddMethod(DualObject)
  # For coverage bots we only build one set of objects and we always set
  # '-fPIC' so we do not need a "special" library.
  if env.Bit('coverage_enabled'):
    env['SHARED_LIBS_SPECIAL'] = False
  else:
    env['SHARED_LIBS_SPECIAL'] = env.Bit('linux')


# In prebuild mode we ignore the dependencies so that stuff does
# NOT get build again
# Optionally ignore the build process.
DeclareBit('prebuilt', 'Disable all build steps, only support install steps')
pre_base_env.SetBitFromOption('prebuilt', False)


# ----------------------------------------------------------
# HELPERS FOR TEST INVOLVING TRUSTED AND UNTRUSTED ENV
# ----------------------------------------------------------
def GetEmulator(env):
  emulator = ARGUMENTS.get('force_emulator')
  if emulator is None and 'TRUSTED_ENV' in env:
    emulator = env['TRUSTED_ENV'].get('EMULATOR')
  return emulator

def UsingEmulator(env):
  return bool(GetEmulator(env))

pre_base_env.AddMethod(UsingEmulator)


def GetValidator(env, validator):
  # NOTE: that the variable TRUSTED_ENV is set by ExportSpecialFamilyVars()
  if 'TRUSTED_ENV' not in env:
    return None

  if validator is None:
    if env.Bit('build_arm'):
      validator = 'arm-ncval-core'
    else:
      validator = 'ncval'

  trusted_env = env['TRUSTED_ENV']
  return trusted_env.File('${STAGING_DIR}/${PROGPREFIX}%s${PROGSUFFIX}' %
                    validator)


# Perform os.path.abspath rooted at the directory SConstruct resides in.
def SConstructAbsPath(env, path):
  return os.path.normpath(os.path.join(env['MAIN_DIR'], path))

pre_base_env.AddMethod(SConstructAbsPath)


def GetSelLdr(env):
  sel_ldr = ARGUMENTS.get('force_sel_ldr')
  if sel_ldr:
    return env.File(env.SConstructAbsPath(sel_ldr))

  # NOTE: that the variable TRUSTED_ENV is set by ExportSpecialFamilyVars()
  if 'TRUSTED_ENV' not in env:
    return None

  trusted_env = env['TRUSTED_ENV']
  return trusted_env.File('${STAGING_DIR}/${PROGPREFIX}sel_ldr${PROGSUFFIX}')

def GetBootstrap(env):
  if 'TRUSTED_ENV' in env:
    trusted_env = env['TRUSTED_ENV']
    if trusted_env.Bit('linux'):
      return (trusted_env.File('${STAGING_DIR}/nacl_helper_bootstrap'),
              '--r_debug=0xXXXXXXXXXXXXXXXX')
  return None, None

pre_base_env.AddMethod(GetBootstrap)


def GetIrtNexe(env, irt_name='irt'):
  image = ARGUMENTS.get('force_irt')
  if image:
    return env.SConstructAbsPath(image)

  return nacl_irt_env.File('${STAGING_DIR}/irt.nexe')

pre_base_env.AddMethod(GetIrtNexe)


def CommandValidatorTestNacl(env, name, image,
                             validator_flags=None,
                             validator=None,
                             size='medium',
                             **extra):
  validator = GetValidator(env, validator)
  if validator is None:
    print 'WARNING: no validator found. Skipping test %s' % name
    return []

  if validator_flags is None:
    validator_flags = []

  if env.Bit('pnacl_generate_pexe'):
    return []

  command = [validator] + validator_flags + [image]
  return CommandTest(env, name, command, size, **extra)

pre_base_env.AddMethod(CommandValidatorTestNacl)


def ExtractPublishedFiles(env, target_name):
  run_files = ['$STAGING_DIR/' + os.path.basename(published_file.path)
               for published_file in env.GetPublished(target_name, 'run')]
  nexe = '$STAGING_DIR/%s${PROGSUFFIX}' % target_name
  return [env.File(file) for file in run_files + [nexe]]

pre_base_env.AddMethod(ExtractPublishedFiles)


# ----------------------------------------------------------
EXTRA_ENV = ['XAUTHORITY', 'HOME', 'DISPLAY', 'SSH_TTY', 'KRB5CCNAME']

def SetupBrowserEnv(env):
  for var_name in EXTRA_ENV:
    if var_name in os.environ:
      env['ENV'][var_name] = os.environ[var_name]


def GetHeadlessPrefix(env):
  if env.Bit('browser_headless') and env.Bit('host_linux'):
    return ['xvfb-run', '--auto-servernum']
  else:
    # Mac and Windows do not seem to have an equivalent.
    return []


pre_base_env['CHROME_DOWNLOAD_DIR'] = \
    pre_base_env.Dir(ARGUMENTS.get('chrome_binaries_dir', '#chromebinaries'))

def ChromeBinaryArch(env):
  arch = env['BUILD_FULLARCH']
  # Currently there are no 64-bit Chrome binaries for Windows or Mac OS X.
  if (env.Bit('host_windows') or env.Bit('host_mac')) and arch == 'x86-64':
    arch = 'x86-32'
  return arch

pre_base_env.AddMethod(ChromeBinaryArch)


def DownloadedChromeBinary(env):
  if env.Bit('host_linux'):
    os_name = 'linux'
    binary = 'chrome'
  elif env.Bit('host_windows'):
    os_name = 'windows'
    binary = 'chrome.exe'
  elif env.Bit('host_mac'):
    os_name = 'mac'
    binary = 'Chromium.app/Contents/MacOS/Chromium'
  else:
    raise Exception('Unsupported OS')

  arch = env.ChromeBinaryArch()
  return env.File(os.path.join('${CHROME_DOWNLOAD_DIR}',
                               '%s_%s' % (os_name, arch), binary))

pre_base_env.AddMethod(DownloadedChromeBinary)


def ChromeBinary(env):
  if 'chrome_browser_path' in ARGUMENTS:
    return env.File(env.SConstructAbsPath(ARGUMENTS['chrome_browser_path']))
  else:
    return env.DownloadedChromeBinary()

pre_base_env.AddMethod(ChromeBinary)


def GetPPAPIPluginPath(env, allow_64bit_redirect=True):
  if 'force_ppapi_plugin' in ARGUMENTS:
    return env.SConstructAbsPath(ARGUMENTS['force_ppapi_plugin'])
  if env.Bit('mac'):
    fn = env.File('${STAGING_DIR}/ppNaClPlugin')
  else:
    fn = env.File('${STAGING_DIR}/${SHLIBPREFIX}ppNaClPlugin${SHLIBSUFFIX}')
  if allow_64bit_redirect and env.Bit('target_x86_64'):
    # On 64-bit Windows and on Mac, we need the 32-bit plugin because
    # the browser is 32-bit.
    # Unfortunately it is tricky to build the 32-bit plugin (and all the
    # libraries it needs) in a 64-bit build... so we'll assume it has already
    # been built in a previous invocation.
    # TODO(ncbray) better 32/64 builds.
    if env.Bit('windows'):
      fn = env.subst(fn).abspath.replace('-win-x86-64', '-win-x86-32')
    elif env.Bit('mac'):
      fn = env.subst(fn).abspath.replace('-mac-x86-64', '-mac-x86-32')
  return fn

pre_base_env.AddMethod(GetPPAPIPluginPath)


# runnable-ld.so log has following format:
# lib_name => path_to_lib (0x....address)
def ParseLibInfoInRunnableLdLog(line):
  pos = line.find(' => ')
  if pos < 0:
    return None
  lib_name = line[:pos].strip()
  lib_path = line[pos+4:]
  pos1 = lib_path.rfind(' (')
  if pos1 < 0:
    return None
  lib_path = lib_path[:pos1]
  return lib_name, lib_path


# Copy libs and manifest to the target directory.
# source[0] is a manifest file
# source[1] is a .libs file with a list of libs generated by runnable-ld.so
def CopyLibsForExtensionCommand(target, source, env):
  source_manifest = str(source[0])
  target_manifest = str(target[0])
  shutil.copyfile(source_manifest, target_manifest)
  target_dir = os.path.dirname(target_manifest)
  libs_file = open(str(source[1]), 'r')
  for line in libs_file.readlines():
    lib_info = ParseLibInfoInRunnableLdLog(line)
    if lib_info:
      lib_name, lib_path = lib_info
      # Note: This probably does NOT work with pnacl _pexes_ right now, because
      # the NEEDED metadata in the bitcode doesn't have the original file paths.
      # This should probably be done without such knowledge.
      if lib_path == 'NaClMain':
        # This is a fake file name, which we cannot copy.
        continue
      shutil.copyfile(lib_path, os.path.join(target_dir, lib_name))
  shutil.copyfile(env.subst('${NACL_SDK_LIB}/runnable-ld.so'),
                  os.path.join(target_dir, 'runnable-ld.so'))
  libs_file.close()


# Extensions are loaded from directory on disk and so all dynamic libraries
# they use must be copied to extension directory. The option --extra_serving_dir
# does not help us in this case.
def CopyLibsForExtension(env, target_dir, manifest):
  if not env.Bit('nacl_glibc'):
    return env.Install(target_dir, manifest)
  manifest_base_name = os.path.basename(str(env.subst(manifest)))
  lib_list_node = env.File('${STAGING_DIR}/' + manifest_base_name + '.libs')
  nmf_node = env.Command(
      target_dir + '/' + manifest_base_name,
      [manifest, lib_list_node],
      CopyLibsForExtensionCommand)
  return nmf_node

pre_base_env.AddMethod(CopyLibsForExtension)


# Generate manifest from newlib manifest and the list of libs generated by
# runnable-ld.so.
def GenerateManifestFunc(target, source, env):
  # Open the original manifest and parse it.
  source_file = open(str(source[0]), 'r')
  obj = json.load(source_file)
  source_file.close()
  # Open the file with ldd-format list of NEEDED libs and parse it.
  libs_file = open(str(source[1]), 'r')
  lib_names = []
  arch = env.subst('${TARGET_FULLARCH}')
  for line in libs_file.readlines():
    lib_info = ParseLibInfoInRunnableLdLog(line)
    if lib_info:
      lib_name, _ = lib_info
      lib_names.append(lib_name)
  libs_file.close()
  # Inject the NEEDED libs into the manifest.
  if 'files' not in obj:
    obj['files'] = {}
  for lib_name in lib_names:
    obj['files'][lib_name] = {}
    obj['files'][lib_name][arch] = {}
    obj['files'][lib_name][arch]['url'] = lib_name
  # Put what used to be specified under 'program' into 'main.nexe'.
  obj['files']['main.nexe'] = {}
  for k, v in obj['program'].items():
    obj['files']['main.nexe'][k] = v.copy()
    v['url'] = 'runnable-ld.so'
  # Write the new manifest!
  target_file = open(str(target[0]), 'w')
  json.dump(obj, target_file, sort_keys=True, indent=2)
  target_file.close()
  return 0

def GenerateManifestPnacl(env, dest_file, manifest, exe_file):
  return env.Command(
      dest_file,
      ['${GENNMF}', exe_file, manifest],
      # Generate a flat url scheme to simplify file-staging.
      ('${SOURCES[0]} ${SOURCES[1]} -L${NACL_SDK_LIB} -L${LIB_DIR} '
       ' --flat-url-scheme --base-nmf ${SOURCES[2]} -o ${TARGET}'))

def GenerateManifestDynamicLink(env, dest_file, lib_list_file,
                                manifest, exe_file):
  # Run sel_ldr on the nexe to trace the NEEDED libraries.
  lib_list_node = env.Command(
      lib_list_file,
      [GetSelLdr(env),
       '${NACL_SDK_LIB}/runnable-ld.so',
       exe_file,
       '${SCONSTRUCT_DIR}/DEPS'],
      # We ignore the return code using '-' in order to build tests
      # where binaries do not validate.  This is a Scons feature.
      '-${SOURCES[0]} -a -E LD_TRACE_LOADED_OBJECTS=1 ${SOURCES[1]} '
      '--library-path ${NACL_SDK_LIB}:${LIB_DIR} ${SOURCES[2].posix} '
      '> ${TARGET}')
  return env.Command(dest_file,
                     [manifest, lib_list_node],
                     GenerateManifestFunc)[0]


def GenerateSimpleManifestStaticLink(env, dest_file, exe_name):
  def Func(target, source, env):
    archs = ('x86-32', 'x86-64', 'arm')
    nmf_data = {'program': dict((arch, {'url': '%s_%s.nexe' % (exe_name, arch)})
                                for arch in archs)}
    fh = open(target[0].abspath, 'w')
    json.dump(nmf_data, fh, sort_keys=True, indent=2)
    fh.close()
  node = env.Command(dest_file, [], Func)[0]
  # Scons does not track the dependency of dest_file on exe_name or on
  # the Python code above, so we should always recreate dest_file when
  # it is used.
  env.AlwaysBuild(node)
  return node


def ProgramNameForNmf(env, basename):
  """ Create an architecture-specific filename that can be used in an NMF URL.
  """
  if env.Bit('pnacl_generate_pexe'):
    return basename
  else:
    return '%s_%s' % (basename, env.get('TARGET_FULLARCH'))

pre_base_env.AddMethod(ProgramNameForNmf)


def GenerateSimpleManifest(env, dest_file, exe_name):
  if env.Bit('pnacl_generate_pexe'):
    static_manifest = GenerateSimpleManifestStaticLink(
        env, '%s.static' % dest_file, exe_name)
    return GenerateManifestPnacl(env, dest_file, static_manifest,
                                 '${STAGING_DIR}/%s.pexe' %
                                 env.ProgramNameForNmf(exe_name))
  elif env.Bit('nacl_static_link'):
    return GenerateSimpleManifestStaticLink(env, dest_file, exe_name)
  else:
    static_manifest = GenerateSimpleManifestStaticLink(
        env, '%s.static' % dest_file, exe_name)
    return GenerateManifestDynamicLink(
        env, dest_file, '%s.tmp_lib_list' % dest_file, static_manifest,
        '${STAGING_DIR}/%s.nexe' % env.ProgramNameForNmf(exe_name))


# Returns a pair (main program, is_portable), based on the program
# specified in manifest file.
def GetMainProgramFromManifest(env, manifest):
  manifest_file = open(str(env.File(manifest)), 'r')
  obj = json.load(manifest_file)
  manifest_file.close()
  program_dict = obj['program']
  if env.Bit('pnacl_generate_pexe') and 'portable' in program_dict:
    return program_dict['portable']['pnacl-translate']['url']
  else:
    return program_dict[env.subst('${TARGET_FULLARCH}')]['url']


# Returns scons node for generated manifest.
def GeneratedManifestNode(env, manifest):
  manifest = env.subst(manifest)
  manifest_base_name = os.path.basename(manifest)
  main_program = GetMainProgramFromManifest(env, manifest)
  result = env.File('${STAGING_DIR}/' + manifest_base_name)
  # Always generate the manifest for nacl_glibc and pnacl pexes.
  # For nacl_glibc, generating the mapping of shared libraries is non-trivial.
  # For pnacl, the manifest currently hosts a sha for the pexe.
  if not env.Bit('nacl_glibc') and not env.Bit('pnacl_generate_pexe'):
    env.Install('${STAGING_DIR}', manifest)
    return result
  if env.Bit('pnacl_generate_pexe'):
    return GenerateManifestPnacl(
        env,
        '${STAGING_DIR}/' + manifest_base_name,
        manifest,
        env.File('${STAGING_DIR}/' + os.path.basename(main_program)))
  else:
    return GenerateManifestDynamicLink(
        env, '${STAGING_DIR}/' + manifest_base_name,
        # Note that CopyLibsForExtension() assumes that it can find
        # the library list file under this '*.libs' filename.
        '${STAGING_DIR}/' + manifest_base_name + '.libs',
        manifest,
        env.File('${STAGING_DIR}/' + os.path.basename(main_program)))
  return result


def GetPnaclExtensionNode(env):
  """Get the scons node representing the pnacl support files.
  """
  # This is "built" by src/untrusted/pnacl_support_extension/nacl.scons.
  return env.Dir('${STAGING_DIR}/pnacl_all')

pre_base_env.AddMethod(GetPnaclExtensionNode)


# Compares output_file and golden_file.
# If they are different, prints the difference and returns 1.
# Otherwise, returns 0.
def CheckGoldenFile(golden_file, output_file,
                    filter_regex, filter_inverse, filter_group_only):
  golden = open(golden_file).read()
  actual = open(output_file).read()
  if filter_regex is not None:
    actual = test_lib.RegexpFilterLines(
        filter_regex,
        filter_inverse,
        filter_group_only,
        actual)
  if command_tester.DifferentFromGolden(actual, golden, output_file):
    return 1
  return 0


# Returns action that compares output_file and golden_file.
# This action can be attached to the node with
# env.AddPostAction(target, action)
def GoldenFileCheckAction(env, output_file, golden_file,
                          filter_regex=None, filter_inverse=False,
                          filter_group_only=False):
  def ActionFunc(target, source, env):
    return CheckGoldenFile(env.subst(golden_file), env.subst(output_file),
                           filter_regex, filter_inverse, filter_group_only)

  return env.Action(ActionFunc)


def PPAPIBrowserTester(env,
                       target,
                       url,
                       files,
                       nmfs=None,
                       # List of executable basenames to generate
                       # manifest files for.
                       nmf_names=(),
                       map_files=(),
                       extensions=(),
                       mime_types=(),
                       timeout=20,
                       log_verbosity=2,
                       args=[],
                       # list of key/value pairs that are passed to the test
                       test_args=(),
                       # list of "--flag=value" pairs (no spaces!)
                       browser_flags=(),
                       # redirect streams of NaCl program to files
                       nacl_exe_stdin=None,
                       nacl_exe_stdout=None,
                       nacl_exe_stderr=None,
                       python_tester_script=None,
                       **extra):
  if 'TRUSTED_ENV' not in env:
    return []

  # No browser tests run on arm-thumb2
  # Bug http://code.google.com/p/nativeclient/issues/detail?id=2224
  if env.Bit('target_arm_thumb2'):
    return []

  if env.Bit('pnacl_generate_pexe'):
    # We likely prefer to choose the 'portable' field in nmfs in this mode.
    args = args + ['--prefer_portable_in_manifest']

  # Lint the extra arguments that are being passed to the tester.
  special_args = ['--ppapi_plugin', '--sel_ldr', '--irt_library', '--file',
                  '--map_file', '--extension', '--mime_type', '--tool',
                  '--browser_flag', '--test_arg']
  for arg_name in special_args:
    if arg_name in args:
      raise Exception('%s: %r is a test argument provided by the SCons test'
                      ' wrapper, do not specify it as an additional argument' %
                      (target, arg_name))

  env = env.Clone()
  SetupBrowserEnv(env)

  if 'scale_timeout' in ARGUMENTS:
    timeout = timeout * int(ARGUMENTS['scale_timeout'])

  if python_tester_script is None:
    python_tester_script = env.File('${SCONSTRUCT_DIR}/tools/browser_tester'
                             '/browser_tester.py')
  command = GetHeadlessPrefix(env) + [
      '${PYTHON}', python_tester_script,
      '--browser_path', env.ChromeBinary(),
      '--url', url,
      # Fail if there is no response for X seconds.
      '--timeout', str(timeout)]
  if not env.Bit('disable_dynamic_plugin_loading'):
    command.extend(['--ppapi_plugin', GetPPAPIPluginPath(env['TRUSTED_ENV'])])
    command.extend(['--sel_ldr', GetSelLdr(env)])
    bootstrap, _ = GetBootstrap(env)
    if bootstrap is not None:
      command.extend(['--sel_ldr_bootstrap', bootstrap])
  if (not env.Bit('disable_dynamic_plugin_loading') or
      env.Bit('override_chrome_irt')):
    command.extend(['--irt_library', env.GetIrtNexe()])
  for dep_file in files:
    command.extend(['--file', dep_file])
  for extension in extensions:
    command.extend(['--extension', extension])
  if env.Bit('bitcode'):
    pnacl_extension = env.GetPnaclExtensionNode()
    command.extend(['--extension', pnacl_extension])
  for dest_path, dep_file in map_files:
    command.extend(['--map_file', dest_path, dep_file])
  for file_ext, mime_type in mime_types:
    command.extend(['--mime_type', file_ext, mime_type])
  command.extend(['--serving_dir', '${NACL_SDK_LIB}'])
  command.extend(['--serving_dir', '${LIB_DIR}'])
  if not nmfs is None:
    for nmf_file in nmfs:
      generated_manifest = GeneratedManifestNode(env, nmf_file)
      # We need to add generated manifests to the list of default targets.
      # The manifests should be generated even if the tests are not run -
      # the manifests may be needed for manual testing.
      for group in env['COMPONENT_TEST_PROGRAM_GROUPS']:
        env.Alias(group, generated_manifest)
      # Generated manifests are served in the root of the HTTP server
      command.extend(['--file', generated_manifest])
  for nmf_name in nmf_names:
    tmp_manifest = '%s.tmp/%s.nmf' % (target, nmf_name)
    command.extend(['--map_file', '%s.nmf' % nmf_name,
                    GenerateSimpleManifest(env, tmp_manifest, nmf_name)])
  if 'browser_test_tool' in ARGUMENTS:
    command.extend(['--tool', ARGUMENTS['browser_test_tool']])

  # Suppress debugging information on the Chrome waterfall.
  if env.Bit('disable_flaky_tests') and '--debug' in args:
    args.remove('--debug')

  command.extend(args)
  for flag in browser_flags:
    if flag.find(' ') != -1:
      raise Exception('Spaces not allowed in browser_flags: '
                      'use --flag=value instead')
    command.extend(['--browser_flag', flag])
  for key, value in test_args:
    command.extend(['--test_arg', str(key), str(value)])

  # Set a given file to be the nexe's stdin.
  if nacl_exe_stdin is not None:
    command.extend(['--nacl_exe_stdin', env.subst(nacl_exe_stdin['file'])])

  post_actions = []
  side_effects = []
  # Set a given file to be the nexe's stdout or stderr.  The tester also
  # compares this output against a golden file.
  for stream, params in (
      ('stdout', nacl_exe_stdout),
      ('stderr', nacl_exe_stderr)):
    if params is None:
      continue
    stream_file = env.subst(params['file'])
    side_effects.append(stream_file)
    command.extend(['--nacl_exe_' + stream, stream_file])
    if 'golden' in params:
      golden_file = env.subst(params['golden'])
      filter_regex = params.get('filter_regex', None)
      filter_inverse = params.get('filter_inverse', False)
      filter_group_only = params.get('filter_group_only', False)
      post_actions.append(
          GoldenFileCheckAction(
              env, stream_file, golden_file,
              filter_regex, filter_inverse, filter_group_only))

  if ShouldUseVerboseOptions(extra):
    env.MakeVerboseExtraOptions(target, log_verbosity, extra)
  # Heuristic for when to capture output...
  capture_output = (extra.pop('capture_output', False)
                    or 'process_output_single' in extra)
  node = env.CommandTest(target,
                         command,
                         # Set to 'huge' so that the browser tester's timeout
                         # takes precedence over the default of the test_suite.
                         size='huge',
                         capture_output=capture_output,
                         **extra)
  for side_effect in side_effects:
    env.SideEffect(side_effect, node)
  # We can't check output if the test is not run.
  if not env.Bit('do_not_run_tests'):
    for action in post_actions:
      env.AddPostAction(node, action)
  return node

pre_base_env.AddMethod(PPAPIBrowserTester)


# Disabled for ARM because Chrome binaries for ARM are not available.
def PPAPIBrowserTesterIsBroken(env):
  return env.Bit('target_arm') or env.Bit('target_arm_thumb2')

pre_base_env.AddMethod(PPAPIBrowserTesterIsBroken)

# 3D is disabled everywhere
def PPAPIGraphics3DIsBroken(env):
  return True

pre_base_env.AddMethod(PPAPIGraphics3DIsBroken)


def RemovePrefix(string, prefix):
  if not string.startswith(prefix):
    raise AssertionError('%r does not start with %r' % (string, prefix))
  return string[len(prefix):]


def PyAutoTester(env, target, test, files=[], nmf_names=[], log_verbosity=2,
                 extra_chrome_flags=[], args=[]):
  if 'TRUSTED_ENV' not in env:
    return []

  env = env.Clone()
  SetupBrowserEnv(env)
  extra_deps = [env.ChromeBinary()]
  files_subdir = '${STAGING_DIR}/%s.files' % target
  for dep_file in files:
    extra_deps.append(env.Replicate(files_subdir, dep_file))
  for nmf_name in nmf_names:
    dest_nmf = '%s/%s.nmf' % (files_subdir, nmf_name)
    extra_deps.append(GenerateSimpleManifest(env, dest_nmf, nmf_name))

  if env.Bit('host_mac'):
    # On Mac, remove 'Chromium.app/Contents/MacOS/Chromium' from the path.
    pyautolib_dir = env.ChromeBinary().dir.dir.dir.dir
  else:
    # On Windows or Linux, remove 'chrome' or 'chrome.exe' from the path.
    pyautolib_dir = env.ChromeBinary().dir

  if not env.Bit('host_mac'):
    # On Linux, we use the system default version of python (2.6).
    # On Windows, we use a hermetically sealed version of python (also 2.6).
    pyauto_python = ['${PYTHON}']
  else:
    # On Mac, we match the version of python used to build pyautolib (2.6).
    # We also need to force 32-bit Python, because Macs typically have a
    # "fat" 32/64 bit version of Python, and PyAuto is explicitly built for
    # 32-bit Python.
    pyauto_python = ['arch', '-i386', '${PYTHON}']

  # Construct chrome flags as a list and pass them on to pyauto as one string.
  chrome_flags = []
  if not env.Bit('disable_dynamic_plugin_loading'):
    chrome_flags.append('--register-pepper-plugins=%s;application/x-nacl' %
                        GetPPAPIPluginPath(env['TRUSTED_ENV']))
    extra_deps.append(GetPPAPIPluginPath(env['TRUSTED_ENV']))
    chrome_flags.append('--no-sandbox')
  else:
    chrome_flags.append('--enable-nacl')
  # For flags passed in through |extra_chrome_flags|, if they are not already
  # in |chrome_flags| then add them.
  for extra_chrome_flag in extra_chrome_flags:
    if not extra_chrome_flag in chrome_flags:
      chrome_flags.append(extra_chrome_flag)

  # Silence chrome's complaints that we are running without a sandbox and that
  # the chrome source code is missing from the NaCl bots.
  # Note: Fatal errors in chrome will still be logged at this level.
  chrome_flags.append('--log-level=3')

  osenv = []
  if not env.Bit('disable_dynamic_plugin_loading'):
    # Pass the sel_ldr location to Chrome via the NACL_SEL_LDR variable.
    sel_ldr = GetSelLdr(env)
    osenv.append('NACL_SEL_LDR=%s' % sel_ldr)
    extra_deps.append(sel_ldr)
    bootstrap, _ = GetBootstrap(env)
    if bootstrap is not None:
      osenv.append('NACL_SEL_LDR_BOOTSTRAP=%s' % bootstrap)
      extra_deps.append(bootstrap)
  if (not env.Bit('disable_dynamic_plugin_loading') or
      env.Bit('override_chrome_irt')):
    osenv.append('NACL_IRT_LIBRARY=%s' % env.GetIrtNexe())
    extra_deps.append(env.GetIrtNexe())

  # On Posix, make sure that nexe logs do not pollute the console output.
  # Note: If a test fails, the contents of the page are explicitly logged to
  # the console, including any errors the nexe might have encountered.
  if not env.Bit('host_windows'):
    osenv.append('NACL_EXE_STDOUT=%s' %
                 os.environ.get('NACL_EXE_STDOUT', '/dev/null'))
    osenv.append('NACL_EXE_STDERR=%s' %
                 os.environ.get('NACL_EXE_STDERR', '/dev/null'))

  # Enable PPAPI Dev interfaces for testing.
  osenv.append('NACL_ENABLE_PPAPI_DEV=1')

  # Construct a relative path to the staging directory from where pyauto's HTTP
  # server should serve files.
  main_dir = env.subst('${MAIN_DIR}')
  http_data_dir = ('native_client' +
                   RemovePrefix(env.subst(files_subdir), main_dir))

  command = (GetHeadlessPrefix(env) + pyauto_python +
             ['-u', test, pyautolib_dir,
              '--http-data-dir=%s' % http_data_dir,
              '--chrome-flags="%s"' %' '.join(chrome_flags)])
  command.extend(args)

  return env.CommandTest(target,
                         command,
                         # Set to 'huge' so that the browser tester's timeout
                         # takes precedence over the default of the test_suite.
                         size='huge',
                         capture_output=False,
                         osenv=osenv,
                         extra_deps=extra_deps)

pre_base_env.AddMethod(PyAutoTester)


# Disabled for ARM (because Chrome binaries for ARM are not available.)
# Disabled if Python 2.6 is not being used (it is assumed that the version of
# Python used by SCons will be the version of Python used by PyAuto.)  PyAuto
# is compiled against a specific version of Python, and cannot be imported in
# any other version.
def PyAutoTesterIsBroken(env):
  return (PPAPIBrowserTesterIsBroken(env)
          or env.Bit('nacl_glibc')
          or sys.version_info[0] != 2
          or sys.version_info[1] != 6)

pre_base_env.AddMethod(PyAutoTesterIsBroken)


# Disable async surfaway pyauto test suite on newlib until issues it raises on
# bots are fixed.  TODO(nfullagar): re-enable when this suite for newlib when it
# won't break the bots.
def PyAutoTesterSurfawayAsyncIsBroken(env):
  return not env.Bit('nacl_glibc')

pre_base_env.AddMethod(PyAutoTesterSurfawayAsyncIsBroken)

# ----------------------------------------------------------
def DemoSelLdrNacl(env,
                   target,
                   nexe,
                   log_verbosity=2,
                   sel_ldr_flags=['-d'],
                   args=[]):

  sel_ldr = GetSelLdr(env);
  if not sel_ldr:
    print 'WARNING: no sel_ldr found. Skipping test %s' % target
    return []

  deps = [sel_ldr, nexe]
  command = (['${SOURCES[0].abspath}'] + sel_ldr_flags +
             ['-f', '${SOURCES[1].abspath}', '--'] + args)

  # NOTE: since most of the demos use X11 we need to make sure
  #      some env vars are set for tag, val in extra_env:
  for tag in EXTRA_ENV:
    if os.getenv(tag) is not None:
      env['ENV'][tag] = os.getenv(tag)
    else:
      env['ENV'][tag] =  env.subst(None)

  node = env.Command(target, deps, ' '.join(command))
  return node

# NOTE: This will not really work for ARM with user mode qemu.
#       Support would likely require some emulation magic.
pre_base_env.AddMethod(DemoSelLdrNacl)

# ----------------------------------------------------------
def CommandGdbTestNacl(env, name, command,
                       gdb_flags=[],
                       input=None,
                       **extra):
  """Runs a test under NaCl GDB."""


  sel_ldr = GetSelLdr(env)
  if not sel_ldr:
    print 'WARNING: no sel_ldr found. Skipping test %s' % name
    return []

  gdb = nacl_env['GDB']
  command = ([gdb, '-q', '-batch', '-x', input, '--loader', sel_ldr] +
             gdb_flags + command)

  return CommandTest(env, name, command, 'large', **extra)

pre_base_env.AddMethod(CommandGdbTestNacl)


def SelUniversalTest(env, name, nexe, sel_universal_flags=None, **kwargs):
  # The dynamic linker's ability to receive arguments over IPC at
  # startup currently requires it to reject the plugin's first
  # connection, but this interferes with the sel_universal-based
  # testing because sel_universal does not retry the connection.
  # TODO(mseaborn): Fix by retrying the connection or by adding an
  # option to ld.so to disable its argv-over-IPC feature.
  if env.Bit('nacl_glibc') and not env.Bit('nacl_static_link'):
    return []

  if sel_universal_flags is None:
    sel_universal_flags = []

  # When run under qemu, sel_universal must sneak in qemu to the execv
  # call that spawns sel_ldr.
  if env.Bit('target_arm') and env.UsingEmulator():
    sel_universal_flags.append('--command_prefix')
    sel_universal_flags.append(GetEmulator(env))

  if 'TRUSTED_ENV' not in env:
    return []
  sel_universal = env['TRUSTED_ENV'].File(
      '${STAGING_DIR}/${PROGPREFIX}sel_universal${PROGSUFFIX}')

  # Point to sel_ldr using an environment variable.
  sel_ldr = GetSelLdr(env)
  if sel_ldr is None:
    print 'WARNING: no sel_ldr found. Skipping test %s' % name
    return []
  kwargs.setdefault('osenv', []).append('NACL_SEL_LDR=' + sel_ldr.abspath)
  bootstrap, _ = GetBootstrap(env)
  if bootstrap is not None:
    kwargs['osenv'].append('NACL_SEL_LDR_BOOTSTRAP=%s' % bootstrap.abspath)

  node = CommandSelLdrTestNacl(env,
                               name,
                               nexe,
                               loader=sel_universal,
                               sel_ldr_flags=sel_universal_flags,
                               skip_bootstrap=True,
                               **kwargs)
  if not env.Bit('built_elsewhere'):
    env.Depends(node, sel_ldr)
    if bootstrap is not None:
      env.Depends(node, bootstrap)
  return node

pre_base_env.AddMethod(SelUniversalTest)


# ----------------------------------------------------------

def MakeNaClLogOption(env, target):
  """ Make up a filename related to the [target], for use with NACLLOG.
  The file should end up in the build directory (scons-out/...).
  """
  # NOTE: to log to the source directory use file.srcnode().abspath instead.
  # See http://www.scons.org/wiki/File%28%29
  return env.File(target + '.nacllog').abspath

pre_base_env.AddMethod(MakeNaClLogOption)

def MakeVerboseExtraOptions(env, target, log_verbosity, extra):
  """ Generates **extra options that will give access to service runtime logs,
  at a given log_verbosity. Slips the options into the given extra dict. """
  log_file = env.MakeNaClLogOption(target)
  extra['log_file'] = log_file
  extra_env = ['NACLLOG=%s' % log_file,
               'NACLVERBOSITY=%d' % log_verbosity]
  extra['osenv'] = extra.get('osenv', []) + extra_env

pre_base_env.AddMethod(MakeVerboseExtraOptions)

def ShouldUseVerboseOptions(extra):
  """ Heuristic for setting up Verbose NACLLOG options. """
  return ('process_output_single' in extra or
          'log_golden' in extra)

# ----------------------------------------------------------
DeclareBit('tests_use_irt', 'Non-browser tests also load the IRT image', False)

# Translate the given pexe.
def GetTranslatedNexe(env, pexe):
  pexe_name = pexe.abspath
  nexe_name = pexe_name[:pexe_name.index('.pexe')] + '.nexe'
  # Make sure the pexe doesn't get removed by the fake builders when
  # built_elsewhere=1
  env.Precious(pexe)
  node = env.Command(target=nexe_name, source=[pexe_name],
                     action=[Action('${TRANSLATECOM}', '${TRANSLATECOMSTR}')])
  return node

pre_base_env.AddMethod(GetTranslatedNexe)

def CommandSelLdrTestNacl(env, name, nexe,
                          args = None,
                          log_verbosity=2,
                          sel_ldr_flags=None,
                          loader=None,
                          size='medium',
                          # True for *.nexe statically linked with glibc
                          glibc_static=False,
                          uses_ppapi=False,
                          skip_bootstrap=False,
                          wrapper_program_prefix=None,
                          # e.g., [ 'python', 'time_check.py', '--' ]
                          **extra):
  # Disable all sel_ldr tests for windows under coverage.
  # Currently several .S files block sel_ldr from being instrumented.
  # See http://code.google.com/p/nativeclient/issues/detail?id=831
  if ('TRUSTED_ENV' in env and
      env['TRUSTED_ENV'].Bit('coverage_enabled') and
      env['TRUSTED_ENV'].Bit('windows')):
    return []

  if (env.Bit('pnacl_generate_pexe') and
      env['NACL_BUILD_FAMILY'] != 'TRUSTED' and
      (env.Bit('translate_in_build_step') or not env.Bit('do_not_run_tests'))):
    # The nexe is actually a pexe.  Translate it before we run it.
    nexe = GetTranslatedNexe(env, nexe)
  command = [nexe]
  if args is not None:
    command += args

  if loader is None:
    loader = GetSelLdr(env)
    if loader is None:
      print 'WARNING: no sel_ldr found. Skipping test %s' % name
      return []

  # Avoid problems with [] as default arguments
  if sel_ldr_flags is None:
    sel_ldr_flags = []

  # Disable the validator if running a GLibC test under Valgrind.
  # http://code.google.com/p/nativeclient/issues/detail?id=1799
  if env.IsRunningUnderValgrind() and env.Bit('nacl_glibc'):
    sel_ldr_flags += ['-cc']

  # Skip platform qualification checks on configurations with known issues.
  if GetEmulator(env) or env.IsRunningUnderValgrind() or env.Bit('asan'):
    sel_ldr_flags += ['-Q']

  # The glibc modifications only make sense for nacl_env tests.
  # But this function gets used by some base_env (i.e. src/trusted/...)
  # tests too.  Don't add the --nacl_glibc changes to the command
  # line for those cases.
  if env.Bit('nacl_glibc') and env['NACL_BUILD_FAMILY'] != 'TRUSTED':
    if not glibc_static and not env.Bit('nacl_static_link'):
      command = ['${NACL_SDK_LIB}/runnable-ld.so',
                 # Locally-built shared libraries come from ${LIB_DIR}
                 # while toolchain-provided ones come from ${NACL_SDK_LIB}.
                 '--library-path', '${LIB_DIR}:${NACL_SDK_LIB}'] + command
    # Enable file access.
    sel_ldr_flags += ['-a']

  if env.Bit('tests_use_irt') or uses_ppapi:
    sel_ldr_flags += ['-B', nacl_env.GetIrtNexe()]

  if skip_bootstrap:
    bootstrap, bootstrap_arg = None, None
  else:
    bootstrap, bootstrap_arg = GetBootstrap(env)
  if bootstrap is None:
    loader_cmd = [loader]
  else:
    loader_cmd = [bootstrap, loader, bootstrap_arg]

  command = loader_cmd + sel_ldr_flags + ['--'] + command

  if ShouldUseVerboseOptions(extra):
    env.MakeVerboseExtraOptions(name, log_verbosity, extra)

  node = CommandTest(env, name, command, size, posix_path=True,
                     wrapper_program_prefix=wrapper_program_prefix, **extra)
  if env.Bit('tests_use_irt'):
    env.Alias('irt_tests', node)
  return node

pre_base_env.AddMethod(CommandSelLdrTestNacl)

# ----------------------------------------------------------
TEST_EXTRA_ARGS = ['stdin', 'log_file',
                   'stdout_golden', 'stderr_golden', 'log_golden',
                   'filter_regex', 'filter_inverse', 'filter_group_only',
                   'osenv', 'arch', 'subarch', 'exit_status', 'track_cmdtime',
                   'num_runs', 'process_output_single',
                   'process_output_combined', 'using_nacl_signal_handler',
                   'declares_exit_status']

TEST_TIME_THRESHOLD = {
    'small':   2,
    'medium': 10,
    'large':  60,
    'huge': 1800,
    }

# Valgrind handles SIGSEGV in a way our testing tools do not expect.
UNSUPPORTED_VALGRIND_EXIT_STATUS = ['sigabrt',
                                    'untrusted_sigill' ,
                                    'untrusted_segfault',
                                    'untrusted_sigsegv_or_equivalent',
                                    'trusted_segfault',
                                    'trusted_sigsegv_or_equivalent'];


def GetPerfEnvDescription(env):
  """ Return a string describing architecture, library, etc. options that may
      affect performance. """
  description_list = [env['TARGET_FULLARCH']]
  # Using a list to keep the order consistent.
  bit_to_description = [ ('bitcode', ('pnacl', 'nnacl')),
                         ('translate_fast', ('fast', '')),
                         ('nacl_glibc', ('glibc', 'newlib')),
                         ('nacl_static_link', ('static', 'dynamic')),
                         ]
  for (bit, (descr_yes, descr_no)) in bit_to_description:
    if env.Bit(bit):
      additional = descr_yes
    else:
      additional = descr_no
    if additional:
      description_list.append(additional)
  return '_'.join(description_list)

pre_base_env.AddMethod(GetPerfEnvDescription)


TEST_NAME_MAP = {}

def GetTestName(target):
  key = str(target.path)
  return TEST_NAME_MAP.get(key, key)

pre_base_env['GetTestName'] = GetTestName


def SetTestName(node, name):
  for target in Flatten(node):
    TEST_NAME_MAP[str(target.path)] = name


def CommandTest(env, name, command, size='small', direct_emulation=True,
                extra_deps=[], posix_path=False, capture_output=True,
                wrapper_program_prefix=None, scale_timeout=None,
                **extra):
  if not name.endswith('.out') or name.startswith('$'):
    raise Exception('ERROR: bad test filename for test output %r' % name)

  if env.IsRunningUnderValgrind():
    skip = 'Valgrind'
  elif env.Bit('asan'):
    skip = 'AddressSanitizer'
  else:
    skip = None
  # Valgrind tends to break crash tests by changing the exit status.
  # So far, tests using declares_exit_status are crash tests.  If this
  # changes, we will have to find a way to make declares_exit_status
  # work with Valgrind.
  if (skip is not None and
      (extra.get('exit_status') in UNSUPPORTED_VALGRIND_EXIT_STATUS or
       bool(int(extra.get('declares_exit_status', 0))))):
    print 'Skipping death test "%s" under %s' % (name, skip)
    return []

  name = '${TARGET_ROOT}/test_results/' + name
  # NOTE: using the long version of 'name' helps distinguish opt vs dbg
  max_time = TEST_TIME_THRESHOLD[size]
  if 'scale_timeout' in ARGUMENTS:
    max_time = max_time * int(ARGUMENTS['scale_timeout'])
  if scale_timeout:
    max_time = max_time * scale_timeout

  if env.Bit('nacl_glibc'):
    suite = 'nacl_glibc'
  else:
    suite = 'nacl_newlib'

  script_flags = ['--name', '%s.${GetTestName(TARGET)}' % suite,
                  '--report', name,
                  '--time_warning', str(max_time),
                  '--time_error', str(10 * max_time),
                  ]

  run_under = ARGUMENTS.get('run_under')
  if run_under:
    run_under_extra_args = ARGUMENTS.get('run_under_extra_args')
    if run_under_extra_args:
      run_under = run_under + ',' + run_under_extra_args
    script_flags.append('--run_under');
    script_flags.append(run_under);

  emulator = GetEmulator(env)
  if emulator and direct_emulation:
    command = [emulator] + command

  # test wrapper should go outside of emulators like qemu, since the
  # test wrapper code is not emulated.
  if wrapper_program_prefix is not None:
    command = wrapper_program_prefix + command

  script_flags.append('--perf_env_description')
  script_flags.append(env.GetPerfEnvDescription())

  # Add architecture info.
  extra['arch'] = env['BUILD_ARCHITECTURE']
  extra['subarch'] = env['BUILD_SUBARCH']

  for flag_name, flag_value in extra.iteritems():
    assert flag_name in TEST_EXTRA_ARGS, repr(flag_name)
    if isinstance(flag_value, list):
      # Options to command_tester.py which are actually lists must not be
      # separated by whitespace. This stringifies the lists with a separator
      # char to satisfy command_tester.
      flag_value =  command_tester.StringifyList(flag_value)
    # do not add --flag + |flag_name| |flag_value| if
    # |flag_value| is false (empty).
    if flag_value:
      script_flags.append('--' + flag_name)
      # Make sure flag values are strings (or SCons objects) when building
      # up the command. Right now, this only means convert ints to strings.
      if isinstance(flag_value, int):
        flag_value = str(flag_value)
      script_flags.append(flag_value)

  # Other extra flags
  if not capture_output:
    script_flags.extend(['--capture_output', '0'])

  test_script = env.File('${SCONSTRUCT_DIR}/tools/command_tester.py')
  command = ['${PYTHON}', test_script] + script_flags + command
  return AutoDepsCommand(env, name, command,
                         extra_deps=extra_deps, posix_path=posix_path,
                         disabled=env.Bit('do_not_run_tests'))

pre_base_env.AddMethod(CommandTest)


def FileSizeTest(env, name, envFile, max_size=None):
  """FileSizeTest() returns a scons node like the other XYZTest generators.
  It logs the file size of envFile in a perf-buildbot-recognizable format.
  Optionally, it can cause a test failure if the file is larger than max_size.
  """
  def doSizeCheck(target, source, env):
    filepath = source[0].abspath
    actual_size = os.stat(filepath).st_size
    command_tester.LogPerfResult(name,
                                 env.GetPerfEnvDescription(),
                                 '%.3f' % (actual_size / 1024.0),
                                 'kilobytes')
    # Also get zipped size.
    nexe_file = open(filepath, 'rb')
    zipped_size = len(zlib.compress(nexe_file.read()))
    nexe_file.close()
    command_tester.LogPerfResult(name,
                                 'ZIPPED_' + env.GetPerfEnvDescription(),
                                 '%.3f' % (zipped_size / 1024.0),
                                 'kilobytes')
    # Finally, do the size check.
    if max_size is not None and actual_size > max_size:
      # NOTE: this exception only triggers a failure for this particular test,
      # just like any other test failure.
      raise Exception("File %s larger than expected: expected up to %i, got %i"
                      % (filepath, max_size, actual_size))
  # If 'built_elsewhere', the file should should have already been built.
  # Do not try to built it and/or its pieces.
  if env.Bit('built_elsewhere'):
    env.Ignore(name, envFile)
  return env.Command(name, envFile, doSizeCheck)

pre_base_env.AddMethod(FileSizeTest)

def StripExecutable(env, name, exe):
  """StripExecutable returns a node representing the stripped version of |exe|.
     The stripped version will be given the basename |name|.
     NOTE: for now this only works with the untrusted toolchain.
     STRIP does not appear to be a first-class citizen in SCons and
     STRIP has only been set to point at the untrusted toolchain.
  """
  return env.Command(
      target=name,
      source=[exe],
      action=[Action('${STRIPCOM} ${SOURCES} -o ${TARGET}', '${STRIPCOMSTR}')])

pre_base_env.AddMethod(StripExecutable)


# TODO(ncbray): pretty up the log output when running this builder.
def DisabledCommand(target, source, env):
  pass

pre_base_env['BUILDERS']['DisabledCommand'] = Builder(action=DisabledCommand)


def AutoDepsCommand(env, name, command, extra_deps=[], posix_path=False,
                    disabled=False):
  """AutoDepsCommand() takes a command as an array of arguments.  Each
  argument may either be:

   * a string, or
   * a Scons file object, e.g. one created with env.File() or as the
     result of another build target.

  In the second case, the file is automatically declared as a
  dependency of this command.
  """
  deps = []
  for index, arg in enumerate(command):
    if not isinstance(arg, str):
      if len(Flatten(arg)) != 1:
        # Do not allow this, because it would cause "deps" to get out
        # of sync with the indexes in "command".
        # See http://code.google.com/p/nativeclient/issues/detail?id=1086
        raise AssertionError('Argument to AutoDepsCommand() actually contains '
                             'multiple (or zero) arguments: %r' % arg)
      if posix_path:
        command[index] = '${SOURCES[%d].posix}' % len(deps)
      else:
        command[index] = '${SOURCES[%d].abspath}' % len(deps)
      deps.append(arg)

  # If built_elsewhere, build commands are replaced by no-ops, so make sure
  # the targets don't get removed first
  if env.Bit('built_elsewhere'):
    env.Precious(deps)
  env.Depends(name, extra_deps)

  if disabled:
    return env.DisabledCommand(name, deps)
  else:
    return env.Command(name, deps, ' '.join(command))


pre_base_env.AddMethod(AutoDepsCommand)

# ----------------------------------------------------------
def GetPrintableCommandName(cmd):
  """Look at the first few elements of cmd to derive a suitable command name."""
  cmd_tokens = cmd.split()
  if "python" in cmd_tokens[0] and len(cmd_tokens) >= 2:
    cmd_name = cmd_tokens[1]
  else:
    cmd_name = cmd_tokens[0].split('(')[0]

  # undo some pretty printing damage done by hammer
  cmd_name = cmd_name.replace('________','')
  # use file name part of a path
  return cmd_name.split('/')[-1]


def GetPrintableEnvironmentName(env):
  # use file name part of a obj root path as env name
  return env.subst('${TARGET_ROOT}').split('/')[-1]


def CustomCommandPrinter(cmd, targets, source, env):
  # Abuse the print hook to count the commands that are executed
  if env.Bit('target_stats'):
    cmd_name = GetPrintableCommandName(cmd)
    env_name = GetPrintableEnvironmentName(env)
    CMD_COUNTER[cmd_name] = CMD_COUNTER.get(cmd_name, 0) + 1
    ENV_COUNTER[env_name] = ENV_COUNTER.get(env_name, 0) + 1

  if env.Bit('pp'):
    # Our pretty printer
    if targets:
      cmd_name = GetPrintableCommandName(cmd)
      env_name = GetPrintableEnvironmentName(env)
      sys.stdout.write('[%s] [%s] %s\n' % (cmd_name, env_name,
                                           targets[0].get_path()))
  else:
    # The SCons default (copied from print_cmd_line in Action.py)
    sys.stdout.write(cmd + u'\n')

pre_base_env.Append(PRINT_CMD_LINE_FUNC=CustomCommandPrinter)



# ----------------------------------------------------------
def GetAbsDirArg(env, argument, target):
  """Fetch the named command-line argument and turn it into an absolute
directory name.  If the argument is missing, raise a UserError saying
that the given target requires that argument be given."""
  dir = ARGUMENTS.get(argument)
  if not dir:
    raise UserError('%s must be set when invoking %s' % (argument, target))
  return os.path.join(env.Dir('$MAIN_DIR').abspath, dir)

pre_base_env.AddMethod(GetAbsDirArg)

# ----------------------------------------------------------
pre_base_env.Append(
    CPPDEFINES = [
        ['NACL_BUILD_ARCH', '${BUILD_ARCHITECTURE}' ],
        ['NACL_BUILD_SUBARCH', '${BUILD_SUBARCH}' ],
        ],
    )

def MakeGTestEnv(env):
  # Create an environment to run unit tests using Gtest.
  gtest_env = env.Clone()

  # Define compile-time flag that communicates that we are compiling in the test
  # environment (rather than for the TCB).
  if gtest_env['NACL_BUILD_FAMILY'] == 'TRUSTED':
    gtest_env.Append(CCFLAGS=['-DNACL_TRUSTED_BUT_NOT_TCB'])

  # This is necessary for unittest_main.c which includes gtest/gtest.h
  # The problem is that gtest.h includes other files expecting the
  # include path to be set.
  gtest_env.Prepend(CPPPATH=['${SOURCE_ROOT}/testing/gtest/include'])

  # gtest does not compile with our stringent settings.
  if gtest_env.Bit('linux') or gtest_env.Bit('mac'):
    # because of: gtest-typed-test.h:236:46: error:
    # anonymous variadic macros were introduced in C99
    gtest_env.FilterOut(CCFLAGS=['-pedantic'])
  gtest_env.FilterOut(CXXFLAGS=['-fno-rtti', '-Weffc++'])

  # gtest is incompatible with static linking due to obscure libstdc++
  # linking interactions.
  # See http://code.google.com/p/nativeclient/issues/detail?id=1987
  gtest_env.FilterOut(LINKFLAGS=['-static'])

  gtest_env.Append(LIBS=['gtest'])
  return gtest_env

pre_base_env.AddMethod(MakeGTestEnv)

def MakeBaseTrustedEnv():
  base_env = MakeArchSpecificEnv()
  base_env.Append(
    BUILD_SUBTYPE = '',
    CPPDEFINES = [
      ['NACL_TARGET_ARCH', '${TARGET_ARCHITECTURE}' ],
      ['NACL_TARGET_SUBARCH', '${TARGET_SUBARCH}' ],
      ['NACL_STANDALONE', 1],
      ],
    CPPPATH = [
      '${SOURCE_ROOT}',
    ],

    EXTRA_CFLAGS = [],
    EXTRA_CXXFLAGS = [],
    EXTRA_LIBS = [],
    CFLAGS = ['${EXTRA_CFLAGS}'],
    CXXFLAGS = ['${EXTRA_CXXFLAGS}'],
  )
  if base_env.Bit('ncval_testing'):
    base_env.Append(CPPDEFINES = ['NCVAL_TESTING'])
  if base_env.Bit('target_arm_thumb2'):
    base_env.Append(CPPDEFINES = ['NACL_TARGET_ARM_THUMB2_MODE=1'])

  base_env.Append(
    BUILD_SCONSCRIPTS = ExtendFileList([
      # KEEP THIS SORTED PLEASE
      'src/shared/gio/build.scons',
      'src/shared/imc/build.scons',
      'src/shared/platform/build.scons',
      'src/shared/srpc/build.scons',
      'src/shared/utils/build.scons',
      'src/third_party_mod/gtest/build.scons',
      'src/third_party_mod/jsoncpp/build.scons',
      'src/tools/validator_tools/build.scons',
      'src/trusted/debug_stub/build.scons',
      'src/trusted/desc/build.scons',
      'src/trusted/fault_injection/build.scons',
      'src/trusted/gdb_rsp/build.scons',
      'src/trusted/gio/build.scons',
      'src/trusted/interval_multiset/build.scons',
      'src/trusted/manifest_name_service_proxy/build.scons',
      'src/trusted/nacl_base/build.scons',
      'src/trusted/nonnacl_util/build.scons',
      'src/trusted/perf_counter/build.scons',
      'src/trusted/platform_qualify/build.scons',
      'src/trusted/python_bindings/build.scons',
      'src/trusted/reverse_service/build.scons',
      'src/trusted/sel_universal/build.scons',
      'src/trusted/service_runtime/build.scons',
      'src/trusted/simple_service/build.scons',
      'src/trusted/threading/build.scons',
      'src/trusted/validator/build.scons',
      'src/trusted/validator_ragel/build.scons',
      'src/trusted/validator/x86/build.scons',
      'src/trusted/validator/x86/decoder/build.scons',
      'src/trusted/validator/x86/decoder/generator/build.scons',
      'src/trusted/validator/x86/ncval_reg_sfi/build.scons',
      'src/trusted/validator/x86/ncval_seg_sfi/build.scons',
      'src/trusted/validator/x86/ncval_seg_sfi/generator/build.scons',
      'src/trusted/validator/x86/32/build.scons',
      'src/trusted/validator/x86/64/build.scons',
      'src/trusted/validator_x86/build.scons',
      'src/trusted/weak_ref/build.scons',
      'tests/python_version/build.scons',
      'tests/srpc_message/build.scons',
      'tests/tools/build.scons',
      'tests/unittests/shared/srpc/build.scons',
      'tests/unittests/shared/imc/build.scons',
      'tests/unittests/shared/platform/build.scons',
      'tests/unittests/trusted/asan/build.scons',
      'tests/unittests/trusted/platform_qualify/build.scons',
      'tests/unittests/trusted/service_runtime/build.scons',
      'installer/build.scons'
      ], ppapi_scons_files['trusted_scons_files'])
    )

  base_env.AddMethod(SDKInstallBin)

  # The ARM validator can be built for any target that doesn't use ELFCLASS64.
  if not base_env.Bit('target_x86_64'):
    base_env.Append(
        BUILD_SCONSCRIPTS = [
          'src/trusted/validator_arm/build.scons',
        ])

  base_env.Replace(
      NACL_BUILD_FAMILY = 'TRUSTED',
  )

  # Add optional scons files if present in the directory tree.
  if os.path.exists(pre_base_env.subst('${MAIN_DIR}/supplement/build.scons')):
    base_env.Append(BUILD_SCONSCRIPTS=['${MAIN_DIR}/supplement/build.scons'])

  return base_env


# Select tests to run under coverage build.
pre_base_env['COVERAGE_TARGETS'] = [
    'small_tests', 'medium_tests', 'large_tests',
    'chrome_browser_tests']


pre_base_env.Help("""\
======================================================================
Help for NaCl
======================================================================

Common tasks:
-------------

* cleaning:           scons -c
* building:           scons
* build mandel:       scons --mode=nacl mandel.nexe
* smoke test:         scons --mode=nacl,opt-linux -k pp=1 smoke_tests

* sel_ldr:            scons --mode=opt-linux sel_ldr

* build the plugin:         scons --mode=opt-linux ppNaClPlugin
*      or:                  scons --mode=opt-linux src/trusted/plugin

Targets to build trusted code destined for the SDK:
* build trusted-code tools:     scons build_bin
* install trusted-code tools:   scons install_bin bindir=...
* These default to opt build, or add --mode=dbg-host for debug build.

Targets to build untrusted code destined for the SDK:
* build just libraries:         scons build_lib
* install just headers:         scons install_headers includedir=...
* install just libraries:       scons install_lib libdir=...
* install headers and libraries:scons install includedir=... libdir=...

* dump system info:   scons --mode=nacl,opt-linux dummy

Options:
--------

naclsdk_mode=<mode>   where <mode>:

                    'local': use locally installed sdk kit
                    'download': use the download copy (default)
                    'custom:<path>': use kit at <path>
                    'manual': use settings from env vars NACL_SDK_xxx


--prebuilt          Do not build things, just do install steps

--verbose           Full command line logging before command execution

pp=1                Use command line pretty printing (more concise output)

sysinfo=1           Verbose system info printing

naclsdk_validate=0  Suppress presence check of sdk



Automagically generated help:
-----------------------------
""")

# ---------------------------------------------------------

def SetupClang(env):
  if env.Bit('asan'):
    if env.Bit('host_linux'):
      asan_dir = 'asan_clang_Linux'
    elif env.Bit('host_mac'):
      asan_dir = 'asan_clang_Darwin'
    else:
      print "ERROR: ASan is only available for Linux and Mac"
      sys.exit(-1)
    env['ASAN'] = '${SOURCE_ROOT}/third_party/asan'
    env['CLANG_DIR'] = os.path.join('${ASAN}', asan_dir, 'bin')
    env['CLANG_OPTS'] = '-faddress-sanitizer'
  else:
    env['CLANG_DIR'] = '${SOURCE_ROOT}/third_party/llvm-build/Release+Asserts/bin'
    env['CLANG_OPTS'] = ''

  env['CC'] = '${CLANG_DIR}/clang ${CLANG_OPTS}'
  env['CXX'] = '${CLANG_DIR}/clang++ ${CLANG_OPTS}'

def GenerateOptimizationLevels(env):
  if env.Bit('clang'):
    SetupClang(env)

  # Generate debug variant.
  debug_env = env.Clone(tools = ['target_debug'])
  debug_env['OPTIMIZATION_LEVEL'] = 'dbg'
  debug_env['BUILD_TYPE'] = debug_env.subst('$BUILD_TYPE')
  debug_env['BUILD_DESCRIPTION'] = debug_env.subst('$BUILD_DESCRIPTION')
  AddDualLibrary(debug_env)
  # Add to the list of fully described environments.
  environment_list.append(debug_env)

  # Generate opt variant.
  opt_env = env.Clone(tools = ['target_optimized'])
  opt_env['OPTIMIZATION_LEVEL'] = 'opt'
  opt_env['BUILD_TYPE'] = opt_env.subst('$BUILD_TYPE')
  opt_env['BUILD_DESCRIPTION'] = opt_env.subst('$BUILD_DESCRIPTION')
  AddDualLibrary(opt_env)
  # Add to the list of fully described environments.
  environment_list.append(opt_env)

  return (debug_env, opt_env)

# ----------------------------------------------------------
def SDKInstallBin(env, name, node, target=None):
  """Add the given node to the build_bin and install_bin targets.
It will be installed under the given name with the build target appended.
The optional target argument overrides the setting of what that target is."""
  env.Alias('build_bin', node)
  if 'install_bin' in COMMAND_LINE_TARGETS:
    dir = env.GetAbsDirArg('bindir', 'install_bin')
    if target is None:
      target = env['TARGET_FULLARCH'].replace('-', '_')
    file_name, file_ext = os.path.splitext(name)
    output_name = file_name + '_' + target + file_ext
    install_node = env.InstallAs(os.path.join(dir, output_name), node)
    env.Alias('install_bin', install_node)


def MakeWindowsEnv():
  base_env = MakeBaseTrustedEnv()
  windows_env = base_env.Clone(
      BUILD_TYPE = '${OPTIMIZATION_LEVEL}-win',
      BUILD_TYPE_DESCRIPTION = 'Windows ${OPTIMIZATION_LEVEL} build',
      tools = ['target_platform_windows'],
      # Windows /SAFESEH linking requires either an .sxdata section be
      # present or that @feat.00 be defined as a local, absolute symbol
      # with an odd value.
      ASCOM = '$ASPPCOM /E | $WINASM -defsym @feat.00=1 -o $TARGET',
      PDB = '${TARGET.base}.pdb',
      # Strict doesn't currently work for Windows since some of the system
      # libraries like wsock32 are magical.
      LIBS_STRICT = False,
      TARGET_ARCH='x86_64' if base_env.Bit('build_x86_64') else 'x86',
  )

  windows_env.Append(
      CPPDEFINES = [
          ['NACL_WINDOWS', '1'],
          ['NACL_OSX', '0'],
          ['NACL_LINUX', '0'],
          ['_WIN32_WINNT', '0x0501'],
          ['__STDC_LIMIT_MACROS', '1'],
          ['NOMINMAX', '1'],
          # WIN32 is used by ppapi
          ['WIN32', '1'],
      ],
      LIBS = ['wsock32', 'advapi32'],
      # TODO(bsy) remove 4355 once cross-repo
      # NACL_ALLOW_THIS_IN_INITIALIZER_LIST changes go in.
      CCFLAGS = ['/EHsc', '/WX', '/wd4355', '/wd4800'],
  )

  # This linker option allows us to ensure our builds are compatible with
  # Chromium, which uses it.
  if windows_env.Bit('build_x86_32'):
    windows_env.Append(LINKFLAGS = "/safeseh")

  # We use the GNU assembler (gas) on Windows so that we can use the
  # same .S assembly files on all platforms.  Microsoft's assembler uses
  # a completely different syntax for x86 code.
  if windows_env.Bit('build_x86_64'):
    # This assembler only works for x86-64 code.
    windows_env['WINASM'] = \
        windows_env.File('$SOURCE_ROOT/third_party/mingw-w64/mingw/bin/'
                         'x86_64-w64-mingw32-as.exe').abspath
  else:
    # This assembler only works for x86-32 code.
    windows_env['WINASM'] = \
        windows_env.File('$SOURCE_ROOT/third_party/gnu_binutils/files/'
                         'as').abspath
  return windows_env

(windows_debug_env,
 windows_optimized_env) = GenerateOptimizationLevels(MakeWindowsEnv())

# TODO(bradnelson): This does not quite work yet (c.f.
# service_runtime/build.scons)
# move some debug info close to the binaries where the debugger can find it
# This only matter when we the "wrong" version of VisualStudio is used
n = windows_debug_env.Command('dummy', [], 'echo INSTALL WINDOWS DEBUG DATA')
windows_debug_env.Alias('windows_debug_data_install', n)

n = windows_debug_env.Replicate(
    '${STAGING_DIR}',
    '${VC80_DIR}/vc/redist/Debug_NonRedist/x86/Microsoft.VC80.DebugCRT')

windows_debug_env.Alias('windows_debug_data_install', n)

n = windows_debug_env.Replicate(
    '${STAGING_DIR}',
    '${VC80_DIR}/vc/redist/x86/Microsoft.VC80.CRT')

windows_debug_env.Alias('windows_debug_data_install', n)


def MakeUnixLikeEnv():
  unix_like_env = MakeBaseTrustedEnv()
  # -Wdeclaration-after-statement is desirable because MS studio does
  # not allow declarations after statements in a block, and since much
  # of our code is portable and primarily initially tested on Linux,
  # it'd be nice to get the build error earlier rather than later
  # (building and testing on Linux is faster).
  # TODO(nfullagar): should we consider switching to -std=c99 ?
  unix_like_env.Prepend(
    CFLAGS = ['-std=gnu99', '-Wdeclaration-after-statement' ],
    CCFLAGS = [
        # '-malign-double',
        '-Wall',
        '-pedantic',
        '-Wextra',
        '-Wno-long-long',
        '-Wswitch-enum',
        '-Wsign-compare',
        '-fvisibility=hidden',
        '-fstack-protector',
        ] + werror_flags,
    CXXFLAGS=['-std=c++98'],
    LIBPATH=['/usr/lib'],
    # NOTE: pthread is only neeeded for libppNaClPlugin.so and on arm
    LIBS = ['pthread'],
    CPPDEFINES = [['__STDC_LIMIT_MACROS', '1'],
                  ['__STDC_FORMAT_MACROS', '1'],
                  ],
  )

  if not unix_like_env.Bit('clang'):
    unix_like_env.Append(CCFLAGS=['--param', 'ssp-buffer-size=4'])

  if unix_like_env.Bit('use_libcrypto'):
    unix_like_env.Append(LIBS=['crypto'])
  else:
    unix_like_env.Append(CPPDEFINES=[['USE_CRYPTO', '0']])

  if unix_like_env.Bit('enable_tmpfs_redirect_var'):
    unix_like_env.Append(CPPDEFINES=[['NACL_ENABLE_TMPFS_REDIRECT_VAR', '1']])
  else:
    unix_like_env.Append(CPPDEFINES=[['NACL_ENABLE_TMPFS_REDIRECT_VAR', '0']])
  return unix_like_env


def MakeMacEnv():
  mac_env = MakeUnixLikeEnv().Clone(
      BUILD_TYPE = '${OPTIMIZATION_LEVEL}-mac',
      BUILD_TYPE_DESCRIPTION = 'MacOS ${OPTIMIZATION_LEVEL} build',
      tools = ['target_platform_mac'],
      # TODO(bradnelson): this should really be able to live in unix_like_env
      #                   but can't due to what the target_platform_x module is
      #                   doing.
      LINK = '$CXX',
      PLUGIN_SUFFIX = '.bundle',
  )

  mac_env.Append(
      CCFLAGS=['-mmacosx-version-min=10.5'],
      LINKFLAGS=['-mmacosx-version-min=10.5'],
      CPPDEFINES=[['MAC_OS_X_VERSION_MIN_REQUIRED', 'MAC_OS_X_VERSION_10_5']])
  subarch_flag = '-m%s' % mac_env['BUILD_SUBARCH']
  mac_env.Append(
      CCFLAGS=[subarch_flag, '-fPIC'],
      ASFLAGS=[subarch_flag],
      LINKFLAGS=[subarch_flag, '-fPIC'],
      CPPDEFINES = [['NACL_WINDOWS', '0'],
                    ['NACL_OSX', '1'],
                    ['NACL_LINUX', '0'],
                    # defining _DARWIN_C_SOURCE breaks 10.4
                    #['_DARWIN_C_SOURCE', '1'],
                    #['__STDC_LIMIT_MACROS', '1']
                    ],
  )
  return mac_env

(mac_debug_env, mac_optimized_env) = GenerateOptimizationLevels(MakeMacEnv())


def MakeLinuxEnv():
  linux_env = MakeUnixLikeEnv().Clone(
      BUILD_TYPE = '${OPTIMIZATION_LEVEL}-linux',
      BUILD_TYPE_DESCRIPTION = 'Linux ${OPTIMIZATION_LEVEL} build',
      tools = ['target_platform_linux'],
      # TODO(bradnelson): this should really be able to live in unix_like_env
      #                   but can't due to what the target_platform_x module is
      #                   doing.
      LINK = '$CXX',
  )

  # -m32 and -L/usr/lib32 are needed to do 32-bit builds on 64-bit
  # user-space machines; requires ia32-libs-dev to be installed; or,
  # failing that, ia32-libs and symbolic links *manually* created for
  # /usr/lib32/libssl.so and /usr/lib32/libcrypto.so to the current
  # /usr/lib32/lib*.so.version (tested with ia32-libs 2.2ubuntu11; no
  # ia32-libs-dev was available for testing).
  # Additional symlinks of this sort are needed for gtk,
  # see src/trusted/nonnacl_util/build.scons.

  linux_env.SetDefault(
      # NOTE: look into http://www.scons.org/wiki/DoxygenBuilder
      DOXYGEN = ARGUMENTS.get('DOXYGEN', '/usr/bin/doxygen'),
  )

  # Prepend so we can disable warnings via Append
  linux_env.Prepend(
      CPPDEFINES = [['NACL_WINDOWS', '0'],
                    ['NACL_OSX', '0'],
                    ['NACL_LINUX', '1'],
                    ['_BSD_SOURCE', '1'],
                    ['_POSIX_C_SOURCE', '199506'],
                    ['_XOPEN_SOURCE', '600'],
                    ['_GNU_SOURCE', '1'],
                    ['_LARGEFILE64_SOURCE', '1'],
                    ],
      LIBS = ['rt'],
      )

  if linux_env.Bit('build_x86_32'):
    linux_env.Prepend(
        CCFLAGS = ['-m32', ],
        LINKFLAGS = ['-m32', '-L/usr/lib32', ],
        )
  elif linux_env.Bit('build_x86_64'):
    linux_env.Prepend(
        CCFLAGS = ['-m64', ],
        LINKFLAGS = ['-m64', '-L/usr/lib64', ],
        )
  elif linux_env.Bit('build_arm'):
    if linux_env.Bit('built_elsewhere'):
      def FakeInstall(dest, source, env):
        print 'Not installing', dest
      # Replace build commands with no-ops
      linux_env.Replace(CC='true', CXX='true', LD='true',
                        AR='true', RANLIB='true', INSTALL=FakeInstall)
      # Allow emulation on x86 hosts for testing built_elsewhere flag
      if not platform.machine().startswith('arm'):
        jail = '${SCONSTRUCT_DIR}/toolchain/linux_arm-trusted'
        linux_env.Replace(EMULATOR=jail + '/run_under_qemu_arm')
    else:
      jail = '${SCONSTRUCT_DIR}/toolchain/linux_arm-trusted'
      linux_env.Replace(CC='arm-linux-gnueabi-gcc-4.5',
                        CXX='arm-linux-gnueabi-g++-4.5',
                        LD='arm-linux-gnueabi-ld',
                        EMULATOR=jail + '/run_under_qemu_arm',
                        ASFLAGS=[],
                        LIBPATH=['${LIB_DIR}',
                                 jail + '/usr/lib',
                                 jail + '/lib',
                                 jail + '/usr/lib/arm-linux-gnueabi',
                                 jail + '/lib/arm-linux-gnueabi',
                                 ],
                        LINKFLAGS=['-Wl,-rpath-link=' + jail +
                                   '/lib/arm-linux-gnueabi']
                        )
      linux_env.Prepend(CCFLAGS=['-march=armv7-a',
                                 '-isystem',
                                 jail + '/usr/include',
                                ])
      # /usr/lib makes sense for most configuration except this one
      # No ARM compatible libs can be found there.
      # So this just makes the command lines longer and sometimes results
      # in linker warnings referring to this directory.
      linux_env.FilterOut(LIBPATH=['/usr/lib'])
     # This appears to be needed for sel_universal
    linux_env.Append(LIBS=['dl'])
  else:
    Banner('Strange platform: %s' % BUILD_NAME)

  # These are desireable options for every Linux platform:
  # _FORTIFY_SOURCE: general paranoia "hardening" option for library functions
  # -fPIE/-pie: create a position-independent executable
  # relro/now: "hardening" options for linking
  # noexecstack: ensure that the executable does not get a PT_GNU_STACK
  #              header that causes the kernel to set the READ_IMPLIES_EXEC
  #              personality flag, which disables NX page protection.
  linux_env.Prepend(
      CPPDEFINES=[['-D_FORTIFY_SOURCE', '2']],
      LINKFLAGS=['-pie', '-Wl,-z,relro', '-Wl,-z,now', '-Wl,-z,noexecstack'],
      )
  # The ARM toolchain has a linker that doesn't handle the code its
  # compiler generates under -fPIE.
  if linux_env.Bit('build_arm'):
    linux_env.Prepend(CCFLAGS=['-fPIC'])
    # TODO(mcgrathr): Temporarily punt _FORTIFY_SOURCE for ARM because
    # it causes a libc dependency newer than the old bots have installed.
    linux_env.FilterOut(CPPDEFINES=[['-D_FORTIFY_SOURCE', '2']])
  else:
    linux_env.Prepend(CCFLAGS=['-fPIE'])

  # We always want to use the same flags for .S as for .c because
  # code-generation flags affect the predefines we might test there.
  linux_env.Replace(ASFLAGS=['${CCFLAGS}'])

  return linux_env

(linux_debug_env, linux_optimized_env) = \
    GenerateOptimizationLevels(MakeLinuxEnv())


# Do this before the site_scons/site_tools/naclsdk.py stuff to pass it along.
pre_base_env.Append(
    PNACL_BCLDFLAGS = ARGUMENTS.get('pnacl_bcldflags', '').split(':'))

# ----------------------------------------------------------
# The nacl_env is used to build native_client modules
# using a special tool chain which produces platform
# independent binaries
# NOTE: this loads stuff from: site_scons/site_tools/naclsdk.py
# ----------------------------------------------------------

nacl_env = MakeArchSpecificEnv().Clone(
    tools = ['naclsdk'],
    NACL_BUILD_FAMILY = 'UNTRUSTED',
    BUILD_TYPE = 'nacl',
    BUILD_TYPE_DESCRIPTION = 'NaCl module build',

    ARFLAGS = 'rc',

    # ${SOURCE_ROOT} for #include <ppapi/...>
    CPPPATH = [
      '${SOURCE_ROOT}',
    ],

    EXTRA_CFLAGS = [],
    EXTRA_CXXFLAGS = [],
    EXTRA_LIBS = [],
    EXTRA_LINKFLAGS = ARGUMENTS.get('nacl_linkflags', '').split(':'),

    # always optimize binaries
    CCFLAGS = ['-O2',
               '-fomit-frame-pointer',
               '-Wall',
               '-fdiagnostics-show-option',
               '-pedantic',
               ] +
              werror_flags,

    CFLAGS = ['-std=gnu99',
              ],
    CXXFLAGS = ['-std=gnu++98',
                '-Wno-long-long',
                ],

    # This magic is copied from scons-2.0.1/engine/SCons/Defaults.py
    # where this pattern is used for _LIBDIRFLAGS, which produces -L
    # switches.  Here we are producing a -Wl,-rpath-link,DIR for each
    # element of LIBPATH, i.e. for each -LDIR produced.
    RPATH_LINK_FLAGS = '$( ${_concat(RPATHLINKPREFIX, LIBPATH, RPATHLINKSUFFIX,'
                       '__env__, RDirs, TARGET, SOURCE)} $)',
    RPATHLINKPREFIX = '-Wl,-rpath-link,',
    RPATHLINKSUFFIX = '',

    LIBS = [],
    LINKFLAGS = ['${RPATH_LINK_FLAGS}'],

    # These are settings for in-tree, non-browser tests to use.
    # They use libraries that circumvent the IRT-based implementations
    # in the public libraries.
    NONIRT_LIBS = ['nacl_sys_private'],
    PTHREAD_LIBS = ['pthread_private'],
    DYNCODE_LIBS = ['nacl_dyncode_private'],
    )

# This is the address at which a user executable is expected to place its
# data segment in order to be compatible with the integrated runtime (IRT)
# library.  This address should not be changed lightly.
irt_compatible_rodata_addr = 0x10000000
# This is the address at which the IRT's own code will be located.
# It must be below irt_compatible_rodata and leave enough space for
# the code segment of the IRT.  It should be as close as possible to
# irt_compatible_rodata so as to leave the maximum contiguous area
# available for the dynamic code loading area that falls below it.
# This can be adjusted as necessary for the actual size of the IRT code.
irt_code_addr = irt_compatible_rodata_addr - (4 << 20) # max 4M IRT code
# This is the address at which the IRT's own data will be located.  The
# 32-bit sandboxes limit the address space to 1GB; the initial thread's
# stack sits at the top of the address space and extends down for
# NACL_DEFAULT_STACK_MAX (src/trusted/service_runtime/sel_ldr.h) below.
# So this must be below there, and leave enough space for the IRT's own
# data segment.  It should be as high as possible so as to leave the
# maximum contiguous area available for the user's data and break below.
# This can be adjusted as necessary for the actual size of the IRT data
# (that is RODATA, rounded up to 64k, plus writable data).
# 1G (address space) - 16M (NACL_DEFAULT_STACK_MAX) - 1MB (IRT rodata+data)
irt_data_addr = (1 << 30) - (16 << 20) - (1 << 20)

nacl_env.Replace(
    IRT_DATA_REGION_START = '%#.8x' % irt_compatible_rodata_addr,
    # Load addresses of the IRT's code and data segments.
    IRT_BLOB_CODE_START = '%#.8x' % irt_code_addr,
    IRT_BLOB_DATA_START = '%#.8x' % irt_data_addr,
    )

def TestsUsePublicLibs(env):
  """Change the environment so it uses public libraries for in-tree tests."""
  env.Replace(NONIRT_LIBS=[],
              PTHREAD_LIBS=['pthread'],
              DYNCODE_LIBS=['nacl_dyncode', 'nacl'])

# glibc is incompatible with the private libraries.
# It's probably really only wholly incompatible with libpthread_private,
# but we make it use only the public libraries altogether anyway.
if nacl_env.Bit('nacl_glibc'):
  TestsUsePublicLibs(nacl_env)

# These add on to those set in pre_base_env, above.
nacl_env.Append(
    CPPDEFINES = [
        # This ensures that UINT32_MAX gets defined.
        ['__STDC_LIMIT_MACROS', '1'],
        # This ensures that PRId64 etc. get defined.
        ['__STDC_FORMAT_MACROS', '1'],
        # _GNU_SOURCE ensures that strtof() gets declared.
        ['_GNU_SOURCE', 1],
        # strdup, and other common stuff
        ['_BSD_SOURCE', '1'],
        ['_POSIX_C_SOURCE', '199506'],
        ['_XOPEN_SOURCE', '600'],

        ['DYNAMIC_ANNOTATIONS_ENABLED', '1' ],
        ['DYNAMIC_ANNOTATIONS_PREFIX', 'NACL_' ],
        ],
    )

def FixWindowsAssembler(env):
  if env.Bit('host_windows'):
    # NOTE: This is needed because Windows builds are case-insensitive.
    # Without this we use nacl-as, which doesn't handle include directives, etc.
    env.Replace(ASCOM='${CCCOM}')

FixWindowsAssembler(nacl_env)

# Look in the local include and lib directories before the toolchain's.
nacl_env['INCLUDE_DIR'] = '${TARGET_ROOT}/include'
# Remove the default $LIB_DIR element so that we prepend it without duplication.
# Using PrependUnique alone would let it stay last, where we want it first.
nacl_env.FilterOut(LIBPATH=['${LIB_DIR}'])
nacl_env.PrependUnique(
    CPPPATH = ['${INCLUDE_DIR}'],
    LIBPATH = ['${LIB_DIR}'],
    )

if nacl_env.Bit('bitcode'):
  # passing -O when linking requests LTO, which does additional global
  # optimizations at link time
  nacl_env.Append(LINKFLAGS=['-O3'])

  if nacl_env.Bit('translate_fast'):
    nacl_env.Append(LINKFLAGS=['-Xlinker', '-translate-fast'])

  # With pnacl's clang base/ code uses the "override" keyword.
  nacl_env.Append(CXXFLAGS=['-Wno-c++11-extensions'])

# We use a special environment for building the IRT image because it must
# always use the newlib toolchain, regardless of --nacl_glibc.  We clone
# it from nacl_env here, before too much other cruft has been added.
# We do some more magic below to instantiate it the way we need it.
nacl_irt_env = nacl_env.Clone(
    BUILD_TYPE = 'nacl_irt',
    BUILD_TYPE_DESCRIPTION = 'NaCl IRT build',
    NACL_BUILD_FAMILY = 'UNTRUSTED_IRT',
)

# Since we don't build a whole libpthread for the IRT and thus we can
# omit the directory, we must tell it how to find the pthread.h header.
nacl_irt_env.Append(CPPPATH='${MAIN_DIR}/src/untrusted/pthread')

# Map certain flag bits to suffices on the build output.  This needs to
# happen pretty early, because it affects any concretized directory names.
target_variant_map = [
    ('bitcode', 'pnacl'),
    ('translate_fast', 'fast'),
    ('nacl_pic', 'pic'),
    ('use_sandboxed_translator', 'sbtc'),
    ('nacl_glibc', 'glibc'),
    ('pnacl_generate_pexe', 'pexe')
    ]
for variant_bit, variant_suffix in target_variant_map:
  if nacl_env.Bit(variant_bit):
    nacl_env['TARGET_VARIANT'] += '-' + variant_suffix

if nacl_env.Bit('bitcode'):
  nacl_env['TARGET_VARIANT'] += '-clang'

# TODO(mseaborn): Inline these now that chrome_browser_tests is only
# supported when using the IRT.
nacl_env.Replace(NON_PPAPI_BROWSER_LIBS=[])
nacl_env.Replace(PPAPI_LIBS=['ppapi'])

# TODO(mseaborn): Make nacl-glibc-based static linking work with just
# "-static", without specifying a linker script.
# See http://code.google.com/p/nativeclient/issues/detail?id=1298
def GetLinkerScriptBaseName(env):
  if env.Bit('build_x86_64'):
    return 'elf64_nacl'
  else:
    return 'elf_nacl'

if (nacl_env.Bit('nacl_glibc') and
    nacl_env.Bit('nacl_static_link')):
  if nacl_env.Bit('bitcode'):
    nacl_env.Append(LINKFLAGS=['-static'])
  else:
    # The "-lc" is necessary because libgcc_eh depends on libc but for
    # some reason nacl-gcc is not linking with "--start-group/--end-group".
    nacl_env.Append(LINKFLAGS=[
        '-static',
        '-T', 'ldscripts/%s.x.static' % GetLinkerScriptBaseName(nacl_env),
        '-lc'])

if nacl_env.Bit('running_on_valgrind'):
  nacl_env.Append(CCFLAGS = ['-g', '-Wno-overlength-strings',
                             '-fno-optimize-sibling-calls'],
                  CPPDEFINES = [['DYNAMIC_ANNOTATIONS_ENABLED', '1' ],
                                ['DYNAMIC_ANNOTATIONS_PREFIX', 'NACL_' ]])
  # With GLibC, libvalgrind.so is preloaded at runtime.
  # With Newlib, it has to be linked in.
  if not nacl_env.Bit('nacl_glibc'):
    nacl_env.Append(LINKFLAGS = ['-Wl,-u,have_nacl_valgrind_interceptors'],
                    LIBS = ['valgrind'])

environment_list.append(nacl_env)

if not nacl_env.Bit('nacl_glibc'):
  # These are all specific to nacl-newlib so we do not include them
  # when building against nacl-glibc.  The functionality of
  # pthread/startup/stubs/nosys is provided by glibc.  The valgrind
  # code currently assumes nc_threads.
  nacl_env.Append(
      BUILD_SCONSCRIPTS = [
        ####  ALPHABETICALLY SORTED ####
        'src/untrusted/pthread/nacl.scons',
        'src/untrusted/stubs/nacl.scons',
        'src/untrusted/nosys/nacl.scons',
        ####  ALPHABETICALLY SORTED ####
        ])

nacl_env.Append(
    BUILD_SCONSCRIPTS = ExtendFileList([
    ####  ALPHABETICALLY SORTED ####
    'src/include/nacl/nacl.scons',
    'src/shared/gio/nacl.scons',
    'src/shared/imc/nacl.scons',
    'src/shared/platform/nacl.scons',
    'src/shared/srpc/nacl.scons',
    'src/trusted/service_runtime/nacl.scons',
    'src/trusted/validator_x86/nacl.scons',
    'src/trusted/weak_ref/nacl.scons',
    'src/untrusted/crash_dump/nacl.scons',
    'src/untrusted/irt_stub/nacl.scons',
    'src/untrusted/nacl/nacl.scons',
    'src/untrusted/nacl_ppapi_util/nacl.scons',
    'src/untrusted/pnacl_irt_shim/nacl.scons',
    'src/untrusted/pnacl_support_extension/nacl.scons',
    'src/untrusted/ppapi/nacl.scons',
    'src/untrusted/valgrind/nacl.scons',
    ####  ALPHABETICALLY SORTED ####
    ], ppapi_scons_files['untrusted_scons_files']))

# These are tests that are worthwhile to run in IRT variant only.
irt_only_tests = [
    #### ALPHABETICALLY SORTED ####
    'tests/irt/nacl.scons',
    'tests/random/nacl.scons',
    ]

# These are tests that are worthwhile to run in both IRT and non-IRT variants.
# The nacl_irt_test mode runs them in the IRT variants.
irt_variant_tests = [
    #### ALPHABETICALLY SORTED ####
    'tests/app_lib/nacl.scons',
    'tests/autoloader/nacl.scons',
    'tests/bigalloc/nacl.scons',
    'tests/blob_library_loading/nacl.scons',
    'tests/browser_dynamic_library/nacl.scons',
    'tests/browser_startup_time/nacl.scons',
    'tests/bundle_size/nacl.scons',
    'tests/callingconv/nacl.scons',
    'tests/callingconv_case_by_case/nacl.scons',
    'tests/clock/nacl.scons',
    'tests/computed_gotos/nacl.scons',
    'tests/data_not_executable/nacl.scons',
    'tests/debug_stub/nacl.scons',
    'tests/dup/nacl.scons',
    'tests/dynamic_code_loading/nacl.scons',
    'tests/dynamic_linking/nacl.scons',
    'tests/egyptian_cotton/nacl.scons',
    'tests/environment_variables/nacl.scons',
    'tests/exception_test/nacl.scons',
# Disabled by Brad Chen 4 Sep to try to green Chromium nacl_integrationt tests
#    'tests/fault_injection/nacl.scons',
    'tests/fib/nacl.scons',
    'tests/file/nacl.scons',
    'tests/fixedfeaturecpu/nacl.scons',
    'tests/gc_instrumentation/nacl.scons',
    'tests/glibc_file64_test/nacl.scons',
    'tests/glibc_static_test/nacl.scons',
    'tests/glibc_syscall_wrappers/nacl.scons',
    'tests/glibc_socket_wrappers/nacl.scons',
    'tests/hello_world/nacl.scons',
    'tests/imc_shm_mmap/nacl.scons',
    'tests/infoleak/nacl.scons',
    'tests/libc_free_hello_world/nacl.scons',
    'tests/longjmp/nacl.scons',
    'tests/loop/nacl.scons',
    'tests/mandel/nacl.scons',
    'tests/manifest_file/nacl.scons',
    'tests/math/nacl.scons',
    'tests/memcheck_test/nacl.scons',
    'tests/mmap/nacl.scons',
    'tests/mmap_race_protect/nacl.scons',
    'tests/nacl_log/nacl.scons',
    'tests/nameservice/nacl.scons',
    'tests/nanosleep/nacl.scons',
    'tests/native_worker/nacl.scons',
    'tests/noop/nacl.scons',
    'tests/nrd_xfer/nacl.scons',
    'tests/nthread_nice/nacl.scons',
    'tests/null/nacl.scons',
    'tests/nullptr/nacl.scons',
    'tests/pagesize/nacl.scons',
    'tests/pnacl_abi/nacl.scons',
    'tests/pnacl_client_translator/nacl.scons',
    'tests/postmessage_redir/nacl.scons',
    'tests/redir/nacl.scons',
    'tests/rodata_not_writable/nacl.scons',
    'tests/sbrk/nacl.scons',
    'tests/signal_handler/nacl.scons',
    'tests/sleep/nacl.scons',
    'tests/srpc/nacl.scons',
    'tests/srpc_hw/nacl.scons',
    'tests/srpc_message/nacl.scons',
    'tests/stack_alignment/nacl.scons',
    'tests/stubout_mode/nacl.scons',
    'tests/sysbasic/nacl.scons',
    'tests/syscall_return_sandboxing/nacl.scons',
    'tests/syscalls/nacl.scons',
    'tests/threads/nacl.scons',
    'tests/time/nacl.scons',
    'tests/tls/nacl.scons',
    'tests/tls_perf/nacl.scons',
    'tests/toolchain/nacl.scons',
    'tests/unittests/shared/platform/nacl.scons',
    'tests/untrusted_check/nacl.scons',
    #### ALPHABETICALLY SORTED ####
    # NOTE: The following tests are really IRT-only tests, but they
    # are in this category so that they can generate libraries (which
    # works in nacl_env but not in nacl_irt_test_env) while also
    # adding tests to nacl_irt_test_env.
    'tests/inbrowser_test_runner/nacl.scons',
    'tests/untrusted_crash_dump/nacl.scons',
    ]

# These are tests that are NOT worthwhile to run in an IRT variant.
# In some cases, that's because they are browser tests which always
# use the IRT.  In others, it's because they are special-case tests
# that are incompatible with having an IRT loaded.
nonvariant_tests = ExtendFileList([
    #### ALPHABETICALLY SORTED ####
    'tests/barebones/nacl.scons',
    'tests/chrome_extension/nacl.scons',
    'tests/custom_desc/nacl.scons',
    'tests/exit_status/nacl.scons',
    'tests/imc_sockets/nacl.scons',
    'tests/inbrowser_crash_test/nacl.scons',
    'tests/minnacl/nacl.scons',
    'tests/multiple_sandboxes/nacl.scons',
    'tests/nacl.scons',
    # Potential issue with running them:
    # http://code.google.com/p/nativeclient/issues/detail?id=2092
    # See also the comment in "buildbot/buildbot_standard.py"
    'tests/pnacl_shared_lib_test/nacl.scons',
    'tests/pyauto_nacl/nacl.scons',
    'tests/signal_handler_single_step/nacl.scons',
    'tests/thread_suspension/nacl.scons',
    'tests/translator_size_limits/nacl.scons',
    'tests/trusted_crash/osx_crash_filter/nacl.scons',
    'tests/trusted_crash/osx_crash_forwarding/nacl.scons',
    'tests/trusted_crash/win_crash_in_syscall/nacl.scons',
    'tests/unittests/shared/imc/nacl.scons',
    'tests/unittests/shared/srpc/nacl.scons',
    #### ALPHABETICALLY SORTED ####
    ], ppapi_scons_files['nonvariant_test_scons_files'])

nacl_env.Append(BUILD_SCONSCRIPTS=irt_variant_tests + nonvariant_tests)

# ----------------------------------------------------------
# Possibly install an sdk by downloading it
# ----------------------------------------------------------
# TODO: explore using a less heavy weight mechanism
# NOTE: this uses stuff from: site_scons/site_tools/naclsdk.py
import SCons.Script

SCons.Script.AddOption('--download',
                       dest='download',
                       metavar='DOWNLOAD',
                       default=False,
                       action='store_true',
                       help='deprecated - allow tools to download')

if nacl_env.GetOption('download'):
  print '@@@@ --download is deprecated, use gclient runhooks --force'
  nacl_sync_env = nacl_env.Clone()
  nacl_sync_env['ENV'] = os.environ
  nacl_sync_env.Execute('gclient runhooks --force')


def NaClSharedLibrary(env, lib_name, *args, **kwargs):
  env_shared = env.Clone(COMPONENT_STATIC=False)
  soname = SCons.Util.adjustixes(lib_name, 'lib', '.so')
  env_shared.AppendUnique(SHLINKFLAGS=['-Wl,-soname,%s' % (soname)])
  return env_shared.ComponentLibrary(lib_name, *args, **kwargs)

nacl_env.AddMethod(NaClSharedLibrary)

def NaClSdkLibrary(env, lib_name, *args, **kwargs):
  n = [env.ComponentLibrary(lib_name, *args, **kwargs)]
  if not env.Bit('nacl_disable_shared'):
    n.append(NaClSharedLibrary(env, lib_name, *args, **kwargs))
  return n

nacl_env.AddMethod(NaClSdkLibrary)


# ---------------------------------------------------------------------
# Special environment for untrusted test binaries that use raw syscalls
# ---------------------------------------------------------------------
def RawSyscallObjects(env, sources):
  raw_syscall_env = env.Clone()
  raw_syscall_env.Append(
    CPPDEFINES = [
      ['USE_RAW_SYSCALLS', '1'],
      ['NACL_BUILD_ARCH', '${BUILD_ARCHITECTURE}' ],
      ['NACL_BUILD_SUBARCH', '${BUILD_SUBARCH}' ],
      ['NACL_TARGET_ARCH', '${TARGET_ARCHITECTURE}' ],
      ['NACL_TARGET_SUBARCH', '${TARGET_SUBARCH}' ],
      ],
  )
  objects = []
  for source_file in sources:
    target_name = 'raw_' + os.path.basename(source_file).rstrip('.c')
    object = raw_syscall_env.ComponentObject(target_name,
                                             source_file)
    objects.append(object)
  return objects

nacl_env.AddMethod(RawSyscallObjects)


# The IRT-building environment was cloned from nacl_env, but it should
# ignore the --nacl_glibc, nacl_pic=1 and bitcode=1 switches.
# We have to reinstantiate the naclsdk.py magic after clearing those flags,
# so it regenerates the tool paths right.
# TODO(mcgrathr,bradnelson): could get cleaner if naclsdk.py got folded back in.
nacl_irt_env.ClearBits('nacl_glibc')
nacl_irt_env.ClearBits('nacl_pic')
# For x86 systems we build the IRT using the nnacl TC even when
# the pnacl TC is used otherwise
if nacl_irt_env.Bit('target_x86_64') or nacl_irt_env.Bit('target_x86_32'):
  nacl_irt_env.ClearBits('bitcode')
# The irt is not subject to the pexe contraint!
nacl_irt_env.ClearBits('pnacl_generate_pexe')
# do not build the irt using the sandboxed translator
nacl_irt_env.ClearBits('use_sandboxed_translator')
nacl_irt_env.Tool('naclsdk')
# These are unfortunately clobbered by running Tool, which
# we needed to do to get the destination directory reset.
# We want all the same values from nacl_env.
nacl_irt_env.Replace(EXTRA_CFLAGS=nacl_env['EXTRA_CFLAGS'],
                     EXTRA_CXXFLAGS=nacl_env['EXTRA_CXXFLAGS'],
                     CCFLAGS=nacl_env['CCFLAGS'],
                     CFLAGS=nacl_env['CFLAGS'],
                     CXXFLAGS=nacl_env['CXXFLAGS'])
FixWindowsAssembler(nacl_irt_env)
# Make it find the libraries it builds, rather than the SDK ones.
nacl_irt_env.Replace(LIBPATH='${LIB_DIR}')

if nacl_irt_env.Bit('bitcode'):
  nacl_irt_env.AddBiasForPNaCl()

# All IRT code must avoid direct use of the TLS ABI register, which
# is reserved for user TLS.  Instead, ensure all TLS accesses use a
# call to __nacl_read_tp, which the IRT code overrides to segregate
# IRT-private TLS from user TLS.
# For bitcode, this option is added to the final translate command instead.
if not nacl_irt_env.Bit('bitcode'):
  nacl_irt_env.Append(CCFLAGS=['-mtls-use-call'])

# TODO(mcgrathr): Clean up uses of these methods.
def AddLibraryDummy(env, nodes):
  return nodes
nacl_irt_env.AddMethod(AddLibraryDummy, 'AddLibraryToSdk')

def AddObjectInternal(env, nodes):
  return env.Replicate('${LIB_DIR}', nodes)
nacl_env.AddMethod(AddObjectInternal, 'AddObjectToSdk')
nacl_irt_env.AddMethod(AddObjectInternal, 'AddObjectToSdk')

def IrtNaClSdkLibrary(env, lib_name, *args, **kwargs):
  env.ComponentLibrary(lib_name, *args, **kwargs)
nacl_irt_env.AddMethod(IrtNaClSdkLibrary, 'NaClSdkLibrary')

nacl_irt_env.AddMethod(SDKInstallBin)

# Populate the internal include directory when AddHeaderToSdk
# is used inside nacl_env.
def AddHeaderInternal(env, nodes, subdir='nacl'):
  dir = '${INCLUDE_DIR}'
  if subdir is not None:
    dir += '/' + subdir
  n = env.Replicate(dir, nodes)
  return n

nacl_irt_env.AddMethod(AddHeaderInternal, 'AddHeaderToSdk')

def PublishHeader(env, nodes, subdir):
  if ('install' in COMMAND_LINE_TARGETS or
      'install_headers' in COMMAND_LINE_TARGETS):
    dir = env.GetAbsDirArg('includedir', 'install_headers')
    if subdir is not None:
      dir += '/' + subdir
    n = env.Install(dir, nodes)
    env.Alias('install', env.Alias('install_headers', n))
    return n

def PublishLibrary(env, nodes):
  env.Alias('build_lib', nodes)

  if ('install' in COMMAND_LINE_TARGETS or
      'install_lib' in COMMAND_LINE_TARGETS):
    dir = env.GetAbsDirArg('libdir', 'install_lib')
    n = env.Install(dir, nodes)
    env.Alias('install', env.Alias('install_lib', n))
    return n

def NaClAddHeader(env, nodes, subdir='nacl'):
  n = AddHeaderInternal(env, nodes, subdir)
  PublishHeader(env, n, subdir)
  return n
nacl_env.AddMethod(NaClAddHeader, 'AddHeaderToSdk')

def NaClAddLibrary(env, nodes):
  nodes = env.Replicate('${LIB_DIR}', nodes)
  PublishLibrary(env, nodes)
  return nodes
nacl_env.AddMethod(NaClAddLibrary, 'AddLibraryToSdk')

def NaClAddObject(env, nodes):
  lib_nodes = env.Replicate('${LIB_DIR}', nodes)
  PublishLibrary(env, lib_nodes)
  return lib_nodes
nacl_env.AddMethod(NaClAddObject, 'AddObjectToSdk')

# We want to do this for nacl_env when not under --nacl_glibc,
# but for nacl_irt_env whether or not under --nacl_glibc, so
# we do it separately for each after making nacl_irt_env and
# clearing its Bit('nacl_glibc').
def AddImplicitLibs(env):
  implicit_libs = []

  # Require the pnacl_irt_shim for pnacl x86-64.
  # Use -B to have the compiler look for the fresh libpnacl_irt_shim.a.
  if env.Bit('bitcode') and env.Bit('target_x86_64'):
    implicit_libs += ['libpnacl_irt_shim.a']

  if not env.Bit('nacl_glibc'):
    # These are automatically linked in by the compiler, either directly
    # or via the linker script that is -lc.  In the non-glibc build, we
    # are the ones providing these files, so we need dependencies.
    # The ComponentProgram method (site_scons/site_tools/component_builders.py)
    # adds dependencies on env['IMPLICIT_LIBS'] if that's set.
    if env.Bit('bitcode'):
      implicit_libs += ['libnacl.a']
    else:
      implicit_libs += ['crt1.o',
                        'libnacl.a',
                        'crti.o',
                        'crtn.o']
      # TODO(mcgrathr): multilib nonsense defeats -B!  figure out a better way.
      if GetPlatform('targetplatform') == 'x86-32':
        implicit_libs.append(os.path.join('32', 'crt1.o'))

  if implicit_libs != []:
    env['IMPLICIT_LIBS'] = [env.File(os.path.join('${LIB_DIR}', file))
                            for file in implicit_libs]
    # The -B<dir>/ flag is necessary to tell gcc to look for crt[1in].o there.
    env.Prepend(LINKFLAGS=['-B${LIB_DIR}/'])

AddImplicitLibs(nacl_env)
AddImplicitLibs(nacl_irt_env)

nacl_irt_env.Append(
    BUILD_SCONSCRIPTS = ExtendFileList([
        'src/include/nacl/nacl.scons',
        'src/shared/gio/nacl.scons',
        'src/shared/platform/nacl.scons',
        'src/shared/srpc/nacl.scons',
        'src/untrusted/irt/nacl.scons',
        'src/untrusted/nacl/nacl.scons',
        'src/untrusted/stubs/nacl.scons',
    ], ppapi_scons_files['untrusted_irt_scons_files']))

environment_list.append(nacl_irt_env)

# Since browser_tests already use the IRT normally, those are fully covered
# in nacl_env.  But the non_browser_tests don't use the IRT in nacl_env.
# We want additional variants of those tests with the IRT, so we make
# another environment and repeat them with that adjustment.
nacl_irt_test_env = nacl_env.Clone(
    BUILD_TYPE = 'nacl_irt_test',
    BUILD_TYPE_DESCRIPTION = 'NaCl tests build with IRT',
    NACL_BUILD_FAMILY = 'UNTRUSTED_IRT_TESTS',

    INCLUDE_DIR = nacl_env.Dir('${INCLUDE_DIR}'),
    LIB_DIR = nacl_env.Dir('${LIB_DIR}'),

    BUILD_SCONSCRIPTS = irt_variant_tests + irt_only_tests,
    )
nacl_irt_test_env.SetBits('tests_use_irt')
TestsUsePublicLibs(nacl_irt_test_env)

# If a tests/.../nacl.scons file builds a library, we will just use
# the one already built in nacl_env instead.
def IrtTestDummyLibrary(*args, **kwargs):
  pass
nacl_irt_test_env.AddMethod(IrtTestDummyLibrary, 'ComponentLibrary')

def IrtTestAddNodeToTestSuite(env, node, suite_name, node_name=None,
                              is_broken=False, is_flaky=False,
                              disable_irt_suffix=False):
  # The disable_irt_suffix argument is there for allowing tests
  # defined in nacl_irt_test_env to be part of chrome_browser_tests
  # (rather than part of chrome_browser_tests_irt).
  # TODO(mseaborn): But really, all of chrome_browser_tests should be
  # placed in nacl_irt_test_env rather than in nacl_env.
  if not disable_irt_suffix:
    if node_name is not None:
      node_name += '_irt'
    suite_name = [name + '_irt' for name in suite_name]
  return AddNodeToTestSuite(env, node, suite_name, node_name,
                            is_broken, is_flaky)
nacl_irt_test_env.AddMethod(IrtTestAddNodeToTestSuite, 'AddNodeToTestSuite')

environment_list.append(nacl_irt_test_env)


windows_coverage_env = MakeWindowsEnv().Clone(
    tools = ['code_coverage'],
    BUILD_TYPE = 'coverage-win',
    BUILD_TYPE_DESCRIPTION = 'Windows code coverage build',
    # TODO(bradnelson): switch nacl to common testing process so this won't be
    #    needed.
    MANIFEST_FILE = None,
    COVERAGE_ANALYZER_DIR=r'..\third_party\coverage_analyzer\bin',
    COVERAGE_ANALYZER='$COVERAGE_ANALYZER_DIR\coverage_analyzer.exe',
)
# TODO(bradnelson): switch nacl to common testing process so this won't be
#    needed.
windows_coverage_env['LINKCOM'] = windows_coverage_env.Action([
    windows_coverage_env.get('LINKCOM', []),
    '$COVERAGE_VSINSTR /COVERAGE ${TARGET}'])
windows_coverage_env.Append(LINKFLAGS = ['/NODEFAULTLIB:msvcrt'])
AddDualLibrary(windows_coverage_env)
environment_list.append(windows_coverage_env)

mac_coverage_env = MakeMacEnv().Clone(
    tools = ['code_coverage'],
    BUILD_TYPE = 'coverage-mac',
    BUILD_TYPE_DESCRIPTION = 'MacOS code coverage build',
    # Strict doesnt't currently work for coverage because the path to gcov is
    # magically baked into the compiler.
    LIBS_STRICT = False,
)
AddDualLibrary(mac_coverage_env)
environment_list.append(mac_coverage_env)

linux_coverage_env = linux_debug_env.Clone(
    tools = ['code_coverage'],
    BUILD_TYPE = 'coverage-linux',
    BUILD_TYPE_DESCRIPTION = 'Linux code coverage build',
    # Strict doesnt't currently work for coverage because the path to gcov is
    # magically baked into the compiler.
    LIBS_STRICT = False,
)

linux_coverage_env.FilterOut(CCFLAGS=['-fPIE'])
linux_coverage_env.Append(CCFLAGS=['-fPIC'])

linux_coverage_env['OPTIONAL_COVERAGE_LIBS'] = '$COVERAGE_LIBS'
AddDualLibrary(linux_coverage_env)
environment_list.append(linux_coverage_env)

# ----------------------------------------------------------
# Environment Massaging
# ----------------------------------------------------------
RELEVANT_CONFIG = ['NACL_BUILD_FAMILY',
                   'BUILD_TYPE',
                   'TARGET_ROOT',
                   'OBJ_ROOT',
                   'BUILD_TYPE_DESCRIPTION',
                   ]

MAYBE_RELEVANT_CONFIG = ['BUILD_OS',
                         'BUILD_ARCHITECTURE',
                         'BUILD_SUBARCH',
                         'TARGET_OS',
                         'TARGET_ARCHITECTURE',
                         'TARGET_SUBARCH',
                         ]

def DumpCompilerVersion(cc, env):
  if 'gcc' in cc:
    env.Execute(env.Action('set'))
    env.Execute(env.Action('${CC} --v -c'))
    env.Execute(env.Action('${CC} -print-search-dirs'))
    env.Execute(env.Action('${CC} -print-libgcc-file-name'))
  elif cc.startswith('cl'):
    import subprocess
    try:
      p = subprocess.Popen(env.subst('${CC} /V'),
                           bufsize=1000*1000,
                           stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE)
      stdout, stderr = p.communicate()
      print stderr[0:stderr.find("\r")]
    except WindowsError:
      # If vcvars was not run before running SCons, we won't be able to find
      # the compiler at this point.  SCons has built in functions for finding
      # the compiler, but they haven't run yet.
      print 'Can not find the compiler, assuming SCons will find it later.'
  else:
    print "UNKNOWN COMPILER"


def SanityCheckEnvironments(all_envs):
  # simple completeness check
  for env in all_envs:
    for tag in RELEVANT_CONFIG:
      assert tag in env, repr(tag)
      assert env[tag], repr(env[tag])


def LinkTrustedEnv(selected_envs):
  # Collect build families and ensure that we have only one env per family.
  family_map = {}
  for env in selected_envs:
    family = env['NACL_BUILD_FAMILY']
    if family not in family_map:
      family_map[family] = env
    else:
      msg = 'You are using incompatible environments simultaneously\n'
      msg += '%s vs %s\n' % (env['BUILD_TYPE'],
                             family_map[family]['BUILD_TYPE'])
      msg += ('Please specfy the exact environments you require, e.g. '
              'MODE=dbg-host,nacl')
      raise Exception(msg)

  # Set TRUSTED_ENV so that tests of untrusted code can locate sel_ldr
  # etc.  We set this on trusted envs too because some tests on
  # trusted envs run sel_ldr (e.g. using checked-in binaries).
  if 'TRUSTED' in family_map:
    for env in selected_envs:
      env['TRUSTED_ENV'] = family_map['TRUSTED']
  if 'TRUSTED' not in family_map or 'UNTRUSTED' not in family_map:
    Banner('Warning: "--mode" did not specify both trusted and untrusted '
           'build environments.  As a result, many tests will not be run.')


def DumpEnvironmentInfo(selected_envs):
  if VerboseConfigInfo(pre_base_env):
    Banner("The following environments have been configured")
    for env in selected_envs:
      for tag in RELEVANT_CONFIG:
        assert tag in env, repr(tag)
        print "%s:  %s" % (tag, env.subst(env.get(tag)))
      for tag in MAYBE_RELEVANT_CONFIG:
        print "%s:  %s" % (tag, env.subst(env.get(tag)))
      cc = env.subst('${CC}')
      print 'CC:', cc
      asppcom = env.subst('${ASPPCOM}')
      print 'ASPPCOM:', asppcom
      DumpCompilerVersion(cc, env)
      print
    # TODO(pdox): This only works for linux/X86-64. Is there a better way
    #             of locating the PNaCl toolchain here?
    rev_file = 'toolchain/pnacl_linux_x86_64/REV'
    if os.path.exists(rev_file):
      for line in open(rev_file).read().split('\n'):
        if "Revision:" in line:
          print "PNACL : %s" % line

def PnaclSetEmulatorForSandboxedTranslator(selected_envs):
  # Slip in emulator flags if necessary, for the sandboxed pnacl translator
  # on ARM, once emulator is actually known (vs in naclsdk.py, where it
  # is not yet known).
  for env in selected_envs:
    if (env.Bit('bitcode')
        and env.Bit('use_sandboxed_translator')
        and env.UsingEmulator()):
      # This must modify the LINK command itself, since LINKFLAGS may
      # be filtered (e.g., in barebones tests).
      env.Append(LINK=' --pnacl-use-emulator')
      env.Append(TRANSLATE=' --pnacl-use-emulator')

# ----------------------------------------------------------
# Blank out defaults.
Default(None)

# Apply optional supplement if present in the directory tree.
if os.path.exists(pre_base_env.subst('$MAIN_DIR/supplement/supplement.scons')):
  SConscript('supplement/supplement.scons', exports=['environment_list'])

# print sytem info (optionally)
if VerboseConfigInfo(pre_base_env):
  Banner('SCONS ARGS:' + str(sys.argv))
  os.system(pre_base_env.subst('${PYTHON} tools/sysinfo.py'))

CheckArguments()

SanityCheckEnvironments(environment_list)
selected_envs = FilterEnvironments(environment_list)

# If we are building nacl, build nacl_irt too.  This works around it being
# a separate mode due to the vagaries of scons when we'd really rather it
# not be, while not requiring that every bot command line using --mode be
# changed to list '...,nacl,nacl_irt' explicitly.
if nacl_env in selected_envs:
  selected_envs.append(nacl_irt_env)

# The nacl_irt_test_env requires nacl_env to build things correctly.
if nacl_irt_test_env in selected_envs and nacl_env not in selected_envs:
  selected_envs.append(nacl_env)


DumpEnvironmentInfo(selected_envs)
LinkTrustedEnv(selected_envs)
# This must happen after LinkTrustedEnv, since that is where TRUSTED_ENV
# is finally set, and env.UsingEmulator() checks TRUSTED_ENV for the emulator.
# This must also happen before BuildEnvironments.
PnaclSetEmulatorForSandboxedTranslator(selected_envs)
BuildEnvironments(selected_envs)

# Change default to build everything, but not run tests.
Default(['all_programs', 'all_bundles', 'all_test_programs', 'all_libraries'])

# ----------------------------------------------------------
# Sanity check whether we are ready to build nacl modules
# ----------------------------------------------------------
# NOTE: this uses stuff from: site_scons/site_tools/naclsdk.py
if nacl_env.Bit('naclsdk_validate') and (nacl_env in selected_envs or
                                         nacl_irt_env in selected_envs):
  nacl_env.ValidateSdk()

if BROKEN_TEST_COUNT > 0:
  msg = "There are %d broken tests." % BROKEN_TEST_COUNT
  if GetOption('brief_comstr'):
    msg += " Add --verbose to the command line for more information."
  print msg

# separate warnings from actual build output
Banner('B U I L D - O U T P U T:')
