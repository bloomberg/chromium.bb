#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs all types of tests from one unified interface.

TODO(gkanwar):
* Add options to run Monkey tests.
"""

import collections
import optparse
import os
import sys

from pylib import cmd_helper
from pylib import constants
from pylib import ports
from pylib.base import base_test_result
from pylib.browsertests import dispatch as browsertests_dispatch
from pylib.gtest import dispatch as gtest_dispatch
from pylib.host_driven import run_python_tests as python_dispatch
from pylib.instrumentation import dispatch as instrumentation_dispatch
from pylib.uiautomator import dispatch as uiautomator_dispatch
from pylib.utils import emulator, report_results, run_tests_helper


_SDK_OUT_DIR = os.path.join(constants.DIR_SOURCE_ROOT, 'out')


def AddBuildTypeOption(option_parser):
  """Adds the build type option to |option_parser|."""
  default_build_type = 'Debug'
  if 'BUILDTYPE' in os.environ:
    default_build_type = os.environ['BUILDTYPE']
  option_parser.add_option('--debug', action='store_const', const='Debug',
                           dest='build_type', default=default_build_type,
                           help=('If set, run test suites under out/Debug. '
                                 'Default is env var BUILDTYPE or Debug.'))
  option_parser.add_option('--release', action='store_const',
                           const='Release', dest='build_type',
                           help=('If set, run test suites under out/Release.'
                                 ' Default is env var BUILDTYPE or Debug.'))


def AddEmulatorOptions(option_parser):
  """Adds all emulator-related options to |option_parser|."""

  # TODO(gkanwar): Figure out what we're doing with the emulator setup
  # and determine whether these options should be deprecated/removed.
  option_parser.add_option('-e', '--emulator', dest='use_emulator',
                           action='store_true',
                           help='Run tests in a new instance of emulator.')
  option_parser.add_option('-n', '--emulator-count',
                           type='int', default=1,
                           help=('Number of emulators to launch for '
                                 'running the tests.'))
  option_parser.add_option('--abi', default='armeabi-v7a',
                           help='Platform of emulators to launch.')


def ProcessEmulatorOptions(options):
  """Processes emulator options."""
  if options.use_emulator:
    emulator.DeleteAllTempAVDs()


def AddCommonOptions(option_parser):
  """Adds all common options to |option_parser|."""

  AddBuildTypeOption(option_parser)

  option_parser.add_option('--out-directory', dest='out_directory',
                           help=('Path to the out/ directory, irrespective of '
                                 'the build type. Only for non-Chromium uses.'))
  option_parser.add_option('-c', dest='cleanup_test_files',
                           help='Cleanup test files on the device after run',
                           action='store_true')
  option_parser.add_option('--num_retries', dest='num_retries', type='int',
                           default=2,
                           help=('Number of retries for a test before '
                                 'giving up.'))
  option_parser.add_option('-v',
                           '--verbose',
                           dest='verbose_count',
                           default=0,
                           action='count',
                           help='Verbose level (multiple times for more)')
  profilers = ['devicestatsmonitor', 'chrometrace', 'dumpheap', 'smaps',
               'traceview']
  option_parser.add_option('--profiler', dest='profilers', action='append',
                           choices=profilers,
                           help=('Profiling tool to run during test. Pass '
                                 'multiple times to run multiple profilers. '
                                 'Available profilers: %s' % profilers))
  option_parser.add_option('--tool',
                           dest='tool',
                           help=('Run the test under a tool '
                                 '(use --tool help to list them)'))
  option_parser.add_option('--flakiness-dashboard-server',
                           dest='flakiness_dashboard_server',
                           help=('Address of the server that is hosting the '
                                 'Chrome for Android flakiness dashboard.'))
  option_parser.add_option('--skip-deps-push', dest='push_deps',
                           action='store_false', default=True,
                           help=('Do not push dependencies to the device. '
                                 'Use this at own risk for speeding up test '
                                 'execution on local machine.'))
  # TODO(gkanwar): This option is deprecated. Remove it in the future.
  option_parser.add_option('--exit-code', action='store_true',
                           help=('(DEPRECATED) If set, the exit code will be '
                                 'total number of failures.'))
  # TODO(gkanwar): This option is deprecated. It is currently used to run tests
  # with the FlakyTest annotation to prevent the bots going red downstream. We
  # should instead use exit codes and let the Buildbot scripts deal with test
  # failures appropriately. See crbug.com/170477.
  option_parser.add_option('--buildbot-step-failure',
                           action='store_true',
                           help=('(DEPRECATED) If present, will set the '
                                 'buildbot status as STEP_FAILURE, otherwise '
                                 'as STEP_WARNINGS when test(s) fail.'))
  option_parser.add_option('-d', '--device', dest='test_device',
                           help=('Target device for the test suite '
                                 'to run on.'))


def ProcessCommonOptions(options):
  """Processes and handles all common options."""
  if options.out_directory:
    cmd_helper.OutDirectory.set(options.out_directory)
  run_tests_helper.SetLogLevel(options.verbose_count)


def AddCoreGTestOptions(option_parser, default_timeout=60):
  """Add options specific to the gtest framework to |option_parser|."""

  # TODO(gkanwar): Consolidate and clean up test filtering for gtests and
  # content_browsertests.
  option_parser.add_option('--gtest_filter', dest='test_filter',
                           help='Filter GTests by name.')
  option_parser.add_option('-a', '--test_arguments', dest='test_arguments',
                           help='Additional arguments to pass to the test.')
  # TODO(gkanwar): Most likely deprecate/remove this option once we've pinned
  # down what we're doing with the emulator setup.
  option_parser.add_option('-x', '--xvfb', dest='use_xvfb',
                           action='store_true',
                           help='Use Xvfb around tests (ignored if not Linux).')
  # TODO(gkanwar): Possible deprecate this flag. Waiting on word from Peter
  # Beverloo.
  option_parser.add_option('--webkit', action='store_true',
                           help='Run the tests from a WebKit checkout.')
  option_parser.add_option('--exe', action='store_true',
                           help='If set, use the exe test runner instead of '
                           'the APK.')
  option_parser.add_option('-t', dest='timeout',
                           help='Timeout to wait for each test',
                           type='int',
                           default=default_timeout)


def AddContentBrowserTestOptions(option_parser):
  """Adds Content Browser test options to |option_parser|."""

  option_parser.usage = '%prog content_browsertests [options]'
  option_parser.command_list = []
  option_parser.example = '%prog content_browsertests'

  AddCoreGTestOptions(option_parser)
  AddCommonOptions(option_parser)


def AddGTestOptions(option_parser):
  """Adds gtest options to |option_parser|."""

  option_parser.usage = '%prog gtest [options]'
  option_parser.command_list = []
  option_parser.example = '%prog gtest -s base_unittests'

  option_parser.add_option('-s', '--suite', dest='test_suite',
                           help=('Executable name of the test suite to run '
                                 '(use -s help to list them).'))
  AddCoreGTestOptions(option_parser)
  # TODO(gkanwar): Move these to Common Options once we have the plumbing
  # in our other test types to handle these commands
  AddEmulatorOptions(option_parser)
  AddCommonOptions(option_parser)


def AddJavaTestOptions(option_parser):
  """Adds the Java test options to |option_parser|."""

  option_parser.add_option('-f', '--test_filter', dest='test_filter',
                           help=('Test filter (if not fully qualified, '
                                 'will run all matches).'))
  option_parser.add_option(
      '-A', '--annotation', dest='annotation_str',
      help=('Comma-separated list of annotations. Run only tests with any of '
            'the given annotations. An annotation can be either a key or a '
            'key-values pair. A test that has no annotation is considered '
            '"SmallTest".'))
  option_parser.add_option(
      '-E', '--exclude-annotation', dest='exclude_annotation_str',
      help=('Comma-separated list of annotations. Exclude tests with these '
            'annotations.'))
  option_parser.add_option('-j', '--java_only', action='store_true',
                           default=False, help='Run only the Java tests.')
  option_parser.add_option('-p', '--python_only', action='store_true',
                           default=False,
                           help='Run only the host-driven tests.')
  option_parser.add_option('--screenshot', dest='screenshot_failures',
                           action='store_true',
                           help='Capture screenshots of test failures')
  option_parser.add_option('--save-perf-json', action='store_true',
                           help='Saves the JSON file for each UI Perf test.')
  # TODO(gkanwar): Remove this option. It is not used anywhere.
  option_parser.add_option('--shard_retries', type=int, default=1,
                           help=('Number of times to retry each failure when '
                                 'sharding.'))
  option_parser.add_option('--official-build', help='Run official build tests.')
  option_parser.add_option('--python_test_root',
                           help='Root of the host-driven tests.')
  option_parser.add_option('--keep_test_server_ports',
                           action='store_true',
                           help=('Indicates the test server ports must be '
                                 'kept. When this is run via a sharder '
                                 'the test server ports should be kept and '
                                 'should not be reset.'))
  # TODO(gkanwar): This option is deprecated. Remove it in the future.
  option_parser.add_option('--disable_assertions', action='store_true',
                           help=('(DEPRECATED) Run with java assertions '
                                 'disabled.'))
  option_parser.add_option('--test_data', action='append', default=[],
                           help=('Each instance defines a directory of test '
                                 'data that should be copied to the target(s) '
                                 'before running the tests. The argument '
                                 'should be of the form <target>:<source>, '
                                 '<target> is relative to the device data'
                                 'directory, and <source> is relative to the '
                                 'chromium build directory.'))


def ProcessJavaTestOptions(options, error_func):
  """Processes options/arguments and populates |options| with defaults."""

  if options.java_only and options.python_only:
    error_func('Options java_only (-j) and python_only (-p) '
               'are mutually exclusive.')
  options.run_java_tests = True
  options.run_python_tests = True
  if options.java_only:
    options.run_python_tests = False
  elif options.python_only:
    options.run_java_tests = False

  if not options.python_test_root:
    options.run_python_tests = False

  if options.annotation_str:
    options.annotations = options.annotation_str.split(',')
  elif options.test_filter:
    options.annotations = []
  else:
    options.annotations = ['Smoke', 'SmallTest', 'MediumTest', 'LargeTest']

  if options.exclude_annotation_str:
    options.exclude_annotations = options.exclude_annotation_str.split(',')
  else:
    options.exclude_annotations = []

  if not options.keep_test_server_ports:
    if not ports.ResetTestServerPortAllocation():
      raise Exception('Failed to reset test server port.')


def AddInstrumentationTestOptions(option_parser):
  """Adds Instrumentation test options to |option_parser|."""

  option_parser.usage = '%prog instrumentation [options]'
  option_parser.command_list = []
  option_parser.example = ('%prog instrumentation -I '
                           '--test-apk=ChromiumTestShellTest')

  AddJavaTestOptions(option_parser)
  AddCommonOptions(option_parser)

  option_parser.add_option('-w', '--wait_debugger', dest='wait_for_debugger',
                           action='store_true',
                           help='Wait for debugger.')
  option_parser.add_option('-I', dest='install_apk', action='store_true',
                           help='Install test APK.')
  option_parser.add_option(
      '--test-apk', dest='test_apk',
      help=('The name of the apk containing the tests '
            '(without the .apk extension; e.g. "ContentShellTest"). '
            'Alternatively, this can be a full path to the apk.'))


def ProcessInstrumentationOptions(options, error_func):
  """Processes options/arguments and populate |options| with defaults."""

  ProcessJavaTestOptions(options, error_func)

  if not options.test_apk:
    error_func('--test-apk must be specified.')

  if os.path.exists(options.test_apk):
    # The APK is fully qualified, assume the JAR lives along side.
    options.test_apk_path = options.test_apk
    options.test_apk_jar_path = (os.path.splitext(options.test_apk_path)[0] +
                                 '.jar')
  else:
    options.test_apk_path = os.path.join(_SDK_OUT_DIR,
                                         options.build_type,
                                         constants.SDK_BUILD_APKS_DIR,
                                         '%s.apk' % options.test_apk)
    options.test_apk_jar_path = os.path.join(
        _SDK_OUT_DIR, options.build_type, constants.SDK_BUILD_TEST_JAVALIB_DIR,
        '%s.jar' %  options.test_apk)


def AddUIAutomatorTestOptions(option_parser):
  """Adds UI Automator test options to |option_parser|."""

  option_parser.usage = '%prog uiautomator [options]'
  option_parser.command_list = []
  option_parser.example = (
      '%prog uiautomator --test-jar=chromium_testshell_uiautomator_tests'
      ' --package-name=org.chromium.chrome.testshell')
  option_parser.add_option(
      '--package-name',
      help='The package name used by the apk containing the application.')
  option_parser.add_option(
      '--test-jar', dest='test_jar',
      help=('The name of the dexed jar containing the tests (without the '
            '.dex.jar extension). Alternatively, this can be a full path '
            'to the jar.'))

  AddJavaTestOptions(option_parser)
  AddCommonOptions(option_parser)


def ProcessUIAutomatorOptions(options, error_func):
  """Processes UIAutomator options/arguments."""

  ProcessJavaTestOptions(options, error_func)

  if not options.package_name:
    error_func('--package-name must be specified.')

  if not options.test_jar:
    error_func('--test-jar must be specified.')

  if os.path.exists(options.test_jar):
    # The dexed JAR is fully qualified, assume the info JAR lives along side.
    options.uiautomator_jar = options.test_jar
  else:
    options.uiautomator_jar = os.path.join(
        _SDK_OUT_DIR, options.build_type, constants.SDK_BUILD_JAVALIB_DIR,
        '%s.dex.jar' % options.test_jar)
  options.uiautomator_info_jar = (
      options.uiautomator_jar[:options.uiautomator_jar.find('.dex.jar')] +
      '_java.jar')


def RunTestsCommand(command, options, args, option_parser):
  """Checks test type and dispatches to the appropriate function.

  Args:
    command: String indicating the command that was received to trigger
        this function.
    options: optparse options dictionary.
    args: List of extra args from optparse.
    option_parser: optparse.OptionParser object.

  Returns:
    Integer indicated exit code.
  """

  ProcessCommonOptions(options)

  total_failed = 0
  if command == 'gtest':
    # TODO(gkanwar): See the emulator TODO above -- this call should either go
    # away or become generalized.
    ProcessEmulatorOptions(options)
    total_failed = gtest_dispatch.Dispatch(options)
  elif command == 'content_browsertests':
    total_failed = browsertests_dispatch.Dispatch(options)
  elif command == 'instrumentation':
    ProcessInstrumentationOptions(options, option_parser.error)
    results = base_test_result.TestRunResults()
    if options.run_java_tests:
      results.AddTestRunResults(instrumentation_dispatch.Dispatch(options))
    if options.run_python_tests:
      results.AddTestRunResults(python_dispatch.DispatchPythonTests(options))
    report_results.LogFull(
        results=results,
        test_type='Instrumentation',
        test_package=os.path.basename(options.test_apk),
        annotation=options.annotations,
        build_type=options.build_type,
        flakiness_server=options.flakiness_dashboard_server)
    total_failed += len(results.GetNotPass())
  elif command == 'uiautomator':
    ProcessUIAutomatorOptions(options, option_parser.error)
    results = base_test_result.TestRunResults()
    if options.run_java_tests:
      results.AddTestRunResults(uiautomator_dispatch.Dispatch(options))
    if options.run_python_tests:
      results.AddTestRunResults(python_dispatch.Dispatch(options))
    report_results.LogFull(
        results=results,
        test_type='UIAutomator',
        test_package=os.path.basename(options.test_jar),
        annotation=options.annotations,
        build_type=options.build_type,
        flakiness_server=options.flakiness_dashboard_server)
    total_failed += len(results.GetNotPass())
  else:
    raise Exception('Unknown test type state')

  return total_failed


def HelpCommand(command, options, args, option_parser):
  """Display help for a certain command, or overall help.

  Args:
    command: String indicating the command that was received to trigger
        this function.
    options: optparse options dictionary.
    args: List of extra args from optparse.
    option_parser: optparse.OptionParser object.

  Returns:
    Integer indicated exit code.
  """
  # If we don't have any args, display overall help
  if len(args) < 3:
    option_parser.print_help()
    return 0

  command = args[2]

  if command not in VALID_COMMANDS:
    option_parser.error('Unrecognized command.')

  # Treat the help command as a special case. We don't care about showing a
  # specific help page for itself.
  if command == 'help':
    option_parser.print_help()
    return 0

  VALID_COMMANDS[command].add_options_func(option_parser)
  option_parser.usage = '%prog ' + command + ' [options]'
  option_parser.command_list = None
  option_parser.print_help()

  return 0


# Define a named tuple for the values in the VALID_COMMANDS dictionary so the
# syntax is a bit prettier. The tuple is two functions: (add options, run
# command).
CommandFunctionTuple = collections.namedtuple(
    'CommandFunctionTuple', ['add_options_func', 'run_command_func'])
VALID_COMMANDS = {
    'gtest': CommandFunctionTuple(AddGTestOptions, RunTestsCommand),
    'content_browsertests': CommandFunctionTuple(
        AddContentBrowserTestOptions, RunTestsCommand),
    'instrumentation': CommandFunctionTuple(
        AddInstrumentationTestOptions, RunTestsCommand),
    'uiautomator': CommandFunctionTuple(
        AddUIAutomatorTestOptions, RunTestsCommand),
    'help': CommandFunctionTuple(lambda option_parser: None, HelpCommand)
    }


class CommandOptionParser(optparse.OptionParser):
  """Wrapper class for OptionParser to help with listing commands."""

  def __init__(self, *args, **kwargs):
    self.command_list = kwargs.pop('command_list', [])
    self.example = kwargs.pop('example', '')
    optparse.OptionParser.__init__(self, *args, **kwargs)

  #override
  def get_usage(self):
    normal_usage = optparse.OptionParser.get_usage(self)
    command_list = self.get_command_list()
    example = self.get_example()
    return self.expand_prog_name(normal_usage + example + command_list)

  #override
  def get_command_list(self):
    if self.command_list:
      return '\nCommands:\n  %s\n' % '\n  '.join(sorted(self.command_list))
    return ''

  def get_example(self):
    if self.example:
      return '\nExample:\n  %s\n' % self.example
    return ''

def main(argv):
  option_parser = CommandOptionParser(
      usage='Usage: %prog <command> [options]',
      command_list=VALID_COMMANDS.keys())

  if len(argv) < 2 or argv[1] not in VALID_COMMANDS:
    option_parser.print_help()
    return 0
  command = argv[1]
  VALID_COMMANDS[command].add_options_func(option_parser)
  options, args = option_parser.parse_args(argv)
  exit_code = VALID_COMMANDS[command].run_command_func(
      command, options, args, option_parser)

  # Failures of individual test suites are communicated by printing a
  # STEP_FAILURE message.
  # Returning a success exit status also prevents the buildbot from incorrectly
  # marking the last suite as failed if there were failures in other suites in
  # the batch (this happens because the exit status is a sum of all failures
  # from all suites, but the buildbot associates the exit status only with the
  # most recent step).
  return exit_code


if __name__ == '__main__':
  sys.exit(main(sys.argv))
