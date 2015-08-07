#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A tool used to run a Chrome test executable and process the output.

This script is used by the buildbot slaves. It must be run from the outer
build directory, e.g. chrome-release/build/.

For a list of command-line options, call this script with '--help'.
"""

import copy
import logging
import optparse
import os
import platform
import re
import subprocess
import sys

from common import chromium_utils
from common import gtest_utils

# TODO(crbug.com/403564). We almost certainly shouldn't be importing this.
import config

from slave import annotation_utils
from slave import build_directory
from slave import gtest_slave_utils
from slave import slave_utils
from slave import xvfb

USAGE = '%s [options] test.exe [test args]' % os.path.basename(sys.argv[0])

CHROME_SANDBOX_PATH = '/opt/chromium/chrome_sandbox'

# Directory to write JSON for test results into.
DEST_DIR = 'gtest_results'

# The directory that this script is in.
BASE_DIR = os.path.dirname(os.path.abspath(__file__))

def _LaunchDBus():
  """Launches DBus to work around a bug in GLib.

  Works around a bug in GLib where it performs operations which aren't
  async-signal-safe (in particular, memory allocations) between fork and exec
  when it spawns subprocesses. This causes threads inside Chrome's browser and
  utility processes to get stuck, and this harness to hang waiting for those
  processes, which will never terminate. This doesn't happen on users'
  machines, because they have an active desktop session and the
  DBUS_SESSION_BUS_ADDRESS environment variable set, but it does happen on the
  bots. See crbug.com/309093 for more details.

  Returns:
    True if it actually spawned DBus.
  """
  import platform
  if (platform.uname()[0].lower() == 'linux' and
      'DBUS_SESSION_BUS_ADDRESS' not in os.environ):
    try:
      print 'DBUS_SESSION_BUS_ADDRESS env var not found, starting dbus-launch'
      dbus_output = subprocess.check_output(['dbus-launch']).split('\n')
      for line in dbus_output:
        m = re.match(r'([^=]+)\=(.+)', line)
        if m:
          os.environ[m.group(1)] = m.group(2)
          print ' setting %s to %s' % (m.group(1), m.group(2))
      return True
    except (subprocess.CalledProcessError, OSError) as e:
      print 'Exception while running dbus_launch: %s' % e
  return False


def _ShutdownDBus():
  """Manually kills the previously-launched DBus daemon.

  It appears that passing --exit-with-session to dbus-launch in
  _LaunchDBus(), above, doesn't cause the launched dbus-daemon to shut
  down properly. Manually kill the sub-process using the PID it gave
  us at launch time.

  This function is called when the flag --spawn-dbus is given, and if
  _LaunchDBus(), above, actually spawned the dbus-daemon.
  """
  import signal
  if 'DBUS_SESSION_BUS_PID' in os.environ:
    dbus_pid = os.environ['DBUS_SESSION_BUS_PID']
    try:
      os.kill(int(dbus_pid), signal.SIGTERM)
      print ' killed dbus-daemon with PID %s' % dbus_pid
    except OSError as e:
      print ' error killing dbus-daemon with PID %s: %s' % (dbus_pid, e)
  # Try to clean up any stray DBUS_SESSION_BUS_ADDRESS environment
  # variable too. Some of the bots seem to re-invoke runtest.py in a
  # way that this variable sticks around from run to run.
  if 'DBUS_SESSION_BUS_ADDRESS' in os.environ:
    del os.environ['DBUS_SESSION_BUS_ADDRESS']
    print ' cleared DBUS_SESSION_BUS_ADDRESS environment variable'


def _RunGTestCommand(
    options, command, extra_env, pipes=None):
  """Runs a test, printing and possibly processing the output.

  Args:
    options: Options passed for this invocation of runtest.py.
    command: A list of strings in a command (the command and its arguments).
    extra_env: A dictionary of extra environment variables to set.
    pipes: A list of command string lists which the output will be piped to.

  Returns:
    The process return code.
  """
  env = os.environ.copy()
  if extra_env:
    print 'Additional test environment:'
    for k, v in sorted(extra_env.items()):
      print '  %s=%s' % (k, v)
  env.update(extra_env or {})

  # Trigger bot mode (test retries, redirection of stdio, possibly faster,
  # etc.) - using an environment variable instead of command-line flags because
  # some internal waterfalls run this (_RunGTestCommand) for totally non-gtest
  # code.
  # TODO(phajdan.jr): Clean this up when internal waterfalls are fixed.
  env.update({'CHROMIUM_TEST_LAUNCHER_BOT_MODE': '1'})

  return chromium_utils.RunCommand(command, pipes=pipes, env=env)


def _GetMaster():
  """Return the master name for the current host."""
  return chromium_utils.GetActiveMaster()


def _GetMasterString(master):
  """Returns a message describing what the master is."""
  return '[Running for master: "%s"]' % master


def _GenerateJSONForTestResults(options, log_processor):
  """Generates or updates a JSON file from the gtest results XML and upload the
  file to the archive server.

  The archived JSON file will be placed at:
    www-dir/DEST_DIR/buildname/testname/results.json
  on the archive server. NOTE: This will be deprecated.

  Args:
    options: command-line options that are supposed to have build_dir,
        results_directory, builder_name, build_name and test_output_xml values.
    log_processor: An instance of PerformanceLogProcessor or similar class.

  Returns:
    True upon success, False upon failure.
  """
  results_map = None
  try:
    results_map = gtest_slave_utils.GetResultsMap(log_processor)
  except Exception as e:
    # This error will be caught by the following 'not results_map' statement.
    print 'Error: ', e

  if not results_map:
    print 'No data was available to update the JSON results'
    # Consider this non-fatal.
    return True

  build_dir = os.path.abspath(options.build_dir)
  slave_name = options.builder_name or slave_utils.SlaveBuildName(build_dir)

  generate_json_options = copy.copy(options)
  generate_json_options.build_name = slave_name
  generate_json_options.input_results_xml = options.test_output_xml
  generate_json_options.builder_base_url = '%s/%s/%s/%s' % (
      config.Master.archive_url, DEST_DIR, slave_name, options.test_type)
  generate_json_options.master_name = options.master_class_name or _GetMaster()
  generate_json_options.test_results_server = config.Master.test_results_server

  print _GetMasterString(generate_json_options.master_name)

  generator = None

  try:
    if options.revision:
      generate_json_options.chrome_revision = options.revision
    else:
      generate_json_options.chrome_revision = ''

    if options.webkit_revision:
      generate_json_options.webkit_revision = options.webkit_revision
    else:
      generate_json_options.webkit_revision = ''

    # Generate results JSON file and upload it to the appspot server.
    generator = gtest_slave_utils.GenerateJSONResults(
        results_map, generate_json_options)

  except Exception as e:
    print 'Unexpected error while generating JSON: %s' % e
    sys.excepthook(*sys.exc_info())
    return False

  # The code can throw all sorts of exceptions, including
  # slave.gtest.networktransaction.NetworkTimeout so just trap everything.
  # Earlier versions of this code ignored network errors, so until a
  # retry mechanism is added, continue to do so rather than reporting
  # an error.
  try:
    # Upload results JSON file to the appspot server.
    gtest_slave_utils.UploadJSONResults(generator)
  except Exception as e:
    # Consider this non-fatal for the moment.
    print 'Unexpected error while uploading JSON: %s' % e
    sys.excepthook(*sys.exc_info())

  return True


def _BuildTestBinaryCommand(_build_dir, test_exe_path, options):
  """Builds a command to run a test binary.

  Args:
    build_dir: Path to the tools/build directory.
    test_exe_path: Path to test command binary.
    options: Options passed for this invocation of runtest.py.

  Returns:
    A command, represented as a list of command parts.
  """
  command = [
      test_exe_path,
  ]

  if options.annotate == 'gtest':
    command.append('--test-launcher-bot-mode')

    if options.total_shards and options.shard_index:
      command.extend([
          '--test-launcher-total-shards=%d' % options.total_shards,
          '--test-launcher-shard-index=%d' % (options.shard_index - 1)])

  return command


def _UsingGtestJson(options):
  """Returns True if we're using GTest JSON summary."""
  return (options.annotate == 'gtest' and
          not options.run_python_script and
          not options.run_shell_script)


def _GenerateRunIsolatedCommand(build_dir, test_exe_path, options, command):
  """Converts the command to run through the run isolate script.

  All commands are sent through the run isolated script, in case
  they need to be run in isolate mode.
  """
  run_isolated_test = os.path.join(BASE_DIR, 'runisolatedtest.py')
  isolate_command = [
      sys.executable, run_isolated_test,
      '--test_name', options.test_type,
      '--builder_name', options.build_properties.get('buildername', ''),
      '--checkout_dir', os.path.dirname(os.path.dirname(build_dir)),
  ]
  if options.factory_properties.get('force_isolated'):
    isolate_command += ['--force-isolated']
  isolate_command += [test_exe_path, '--'] + command

  return isolate_command


def _GetSanitizerSymbolizeCommand(strip_path_prefix=None, json_file_name=None):
  script_path = os.path.abspath(os.path.join('src', 'tools', 'valgrind',
                                             'asan', 'asan_symbolize.py'))
  command = [sys.executable, script_path]
  if strip_path_prefix:
    command.append(strip_path_prefix)
  if json_file_name:
    command.append('--test-summary-json-file=%s' % json_file_name)
  return command


def _SymbolizeSnippetsInJSON(options, json_file_name):
  if not json_file_name:
    return
  symbolize_command = _GetSanitizerSymbolizeCommand(
      strip_path_prefix=options.strip_path_prefix,
      json_file_name=json_file_name)
  try:
    p = subprocess.Popen(symbolize_command, stderr=subprocess.PIPE)
    (_, stderr) = p.communicate()
  except OSError as e:
      print 'Exception while symbolizing snippets: %s' % e

  if p.returncode != 0:
    print "Error: failed to symbolize snippets in JSON:\n"
    print stderr


def _Main(options, args, extra_env):
  """Using the target build configuration, run the executable given in the
  first non-option argument, passing any following arguments to that
  executable.

  Args:
    options: Command-line options for this invocation of runtest.py.
    args: Command and arguments for the test.
    extra_env: A dictionary of extra environment variables to set.

  Returns:
    Exit status code.
  """
  if len(args) < 1:
    raise chromium_utils.MissingArgument('Usage: %s' % USAGE)

  xvfb_path = os.path.join(os.path.dirname(sys.argv[0]), '..', '..',
                           'third_party', 'xvfb', platform.architecture()[0])
  special_xvfb_dir = None
  fp_chromeos = options.factory_properties.get('chromeos', None)
  if (fp_chromeos or
      slave_utils.GypFlagIsOn(options, 'use_aura') or
      slave_utils.GypFlagIsOn(options, 'chromeos')):
    special_xvfb_dir = xvfb_path

  build_dir = os.path.normpath(os.path.abspath(options.build_dir))
  bin_dir = os.path.join(build_dir, options.target)
  slave_name = options.slave_name or slave_utils.SlaveBuildName(build_dir)

  test_exe = args[0]
  if options.run_python_script:
    test_exe_path = test_exe
  else:
    test_exe_path = os.path.join(bin_dir, test_exe)

  if not os.path.exists(test_exe_path):
    if options.factory_properties.get('succeed_on_missing_exe', False):
      print '%s missing but succeed_on_missing_exe used, exiting' % (
          test_exe_path)
      return 0
    raise chromium_utils.PathNotFound('Unable to find %s' % test_exe_path)

  if sys.platform == 'linux2':
    # Unset http_proxy and HTTPS_PROXY environment variables.  When set, this
    # causes some tests to hang.  See http://crbug.com/139638 for more info.
    if 'http_proxy' in os.environ:
      del os.environ['http_proxy']
      print 'Deleted http_proxy environment variable.'
    if 'HTTPS_PROXY' in os.environ:
      del os.environ['HTTPS_PROXY']
      print 'Deleted HTTPS_PROXY environment variable.'

    # Path to SUID sandbox binary. This must be installed on all bots.
    extra_env['CHROME_DEVEL_SANDBOX'] = CHROME_SANDBOX_PATH

    extra_env['LD_LIBRARY_PATH'] = ''
    if options.enable_lsan:
      # Use the debug version of libstdc++ under LSan. If we don't, there will
      # be a lot of incomplete stack traces in the reports.
      extra_env['LD_LIBRARY_PATH'] += '/usr/lib/x86_64-linux-gnu/debug:'
    extra_env['LD_LIBRARY_PATH'] += '%s:%s/lib:%s/lib.target' % (
        bin_dir, bin_dir, bin_dir)

  if options.run_shell_script:
    command = ['bash', test_exe_path]
  elif options.run_python_script:
    command = [sys.executable, test_exe]
  else:
    command = _BuildTestBinaryCommand(build_dir, test_exe_path, options)
  command.extend(args[1:])

  # Nuke anything that appears to be stale chrome items in the temporary
  # directory from previous test runs (i.e.- from crashes or unittest leaks).
  slave_utils.RemoveChromeTemporaryFiles()

  log_processor = None
  if _UsingGtestJson(options):
    log_processor = gtest_utils.GTestJSONParser(
        options.build_properties.get('mastername'))

  if options.generate_json_file:
    if os.path.exists(options.test_output_xml):
      # remove the old XML output file.
      os.remove(options.test_output_xml)

  try:
    # TODO(dpranke): checking on test_exe is a temporary hack until we
    # can change the buildbot master to pass --xvfb instead of --no-xvfb
    # for these two steps. See
    # https://code.google.com/p/chromium/issues/detail?id=179814
    start_xvfb = (
        sys.platform == 'linux2' and (
            options.xvfb or
            'layout_test_wrapper' in test_exe or
            'devtools_perf_test_wrapper' in test_exe))
    if start_xvfb:
      xvfb.StartVirtualX(
          slave_name, bin_dir,
          with_wm=(options.factory_properties.get('window_manager', 'True') ==
                   'True'),
          server_dir=special_xvfb_dir)

    if _UsingGtestJson(options):
      json_file_name = log_processor.PrepareJSONFile(
          options.test_launcher_summary_output)
      command.append('--test-launcher-summary-output=%s' % json_file_name)

    pipes = []
    # See the comment in main() regarding offline symbolization.
    if options.use_symbolization_script:
      symbolize_command = _GetSanitizerSymbolizeCommand(
          strip_path_prefix=options.strip_path_prefix)
      pipes = [symbolize_command]

    command = _GenerateRunIsolatedCommand(build_dir, test_exe_path, options,
                                          command)
    result = _RunGTestCommand(options, command, extra_env, pipes=pipes)
  finally:
    if start_xvfb:
      xvfb.StopVirtualX(slave_name)
    if _UsingGtestJson(options):
      if options.use_symbolization_script:
        _SymbolizeSnippetsInJSON(options, json_file_name)
      log_processor.ProcessJSONFile(options.build_dir)

  if options.generate_json_file:
    if not _GenerateJSONForTestResults(options, log_processor):
      return 1

  if options.annotate:
    annotation_utils.annotate(options.test_type, result, log_processor)

  return result


def _ConfigureSanitizerTools(options, args, extra_env):
  if (options.enable_asan or options.enable_tsan or
      options.enable_msan or options.enable_lsan):
    # Instruct GTK to use malloc while running ASan, TSan, MSan or LSan tests.
    extra_env['G_SLICE'] = 'always-malloc'
    extra_env['NSS_DISABLE_ARENA_FREE_LIST'] = '1'
    extra_env['NSS_DISABLE_UNLOAD'] = '1'

  symbolizer_path = os.path.abspath(os.path.join('src', 'third_party',
      'llvm-build', 'Release+Asserts', 'bin', 'llvm-symbolizer'))
  disable_sandbox_flag = '--no-sandbox'
  if args and 'layout_test_wrapper' in args[0]:
    disable_sandbox_flag = '--additional-drt-flag=%s' % disable_sandbox_flag

  # Symbolization of sanitizer reports.
  if sys.platform in ['win32', 'cygwin']:
    # On Windows, the in-process symbolizer works even when sandboxed.
    symbolization_options = []
  elif options.enable_tsan or options.enable_lsan:
    # TSan and LSan are not sandbox-compatible, so we can use online
    # symbolization. In fact, they need symbolization to be able to apply
    # suppressions.
    symbolization_options = ['symbolize=1',
                             'external_symbolizer_path=%s' % symbolizer_path,
                             'strip_path_prefix=%s' % options.strip_path_prefix]
  elif options.enable_asan or options.enable_msan:
    # ASan and MSan use a script for offline symbolization.
    # Important note: when running ASan or MSan with leak detection enabled,
    # we must use the LSan symbolization options above.
    symbolization_options = ['symbolize=0']
    # Set the path to llvm-symbolizer to be used by asan_symbolize.py
    extra_env['LLVM_SYMBOLIZER_PATH'] = symbolizer_path
    options.use_symbolization_script = True

  def AddToExistingEnv(env_dict, key, options_list):
    # Adds a key to the supplied environment dictionary but appends it to
    # existing environment variables if it already contains values.
    assert type(env_dict) is dict
    assert type(options_list) is list
    env_dict[key] = ' '.join(filter(bool, [os.environ.get(key)]+options_list))

  # ThreadSanitizer
  if options.enable_tsan:
    tsan_options = symbolization_options
    AddToExistingEnv(extra_env, 'TSAN_OPTIONS', tsan_options)
    # Disable sandboxing under TSan for now. http://crbug.com/223602.
    args.append(disable_sandbox_flag)

  # LeakSanitizer
  if options.enable_lsan:
    # Symbolization options set here take effect only for standalone LSan.
    lsan_options = symbolization_options
    AddToExistingEnv(extra_env, 'LSAN_OPTIONS', lsan_options)

    # Disable sandboxing under LSan.
    args.append(disable_sandbox_flag)

  # AddressSanitizer
  if options.enable_asan:
    asan_options = symbolization_options
    if options.enable_lsan:
      asan_options += ['detect_leaks=1']
    AddToExistingEnv(extra_env, 'ASAN_OPTIONS', asan_options)

  # MemorySanitizer
  if options.enable_msan:
    msan_options = symbolization_options
    if options.enable_lsan:
      msan_options += ['detect_leaks=1']
    AddToExistingEnv(extra_env, 'MSAN_OPTIONS', msan_options)


def main():
  """Entry point for runtest.py.

  This function:
    (1) Sets up the command-line options.
    (2) Sets environment variables based on those options.
    (3) Delegates to the platform-specific main functions.

  Returns:
    Exit code for this script.
  """
  option_parser = optparse.OptionParser(usage=USAGE)

  # Since the trailing program to run may have has command-line args of its
  # own, we need to stop parsing when we reach the first positional argument.
  option_parser.disable_interspersed_args()

  option_parser.add_option('--target', default='Release',
                           help='build target (Debug or Release)')
  option_parser.add_option('--pass-target', action='store_true', default=False,
                           help='pass --target to the spawned test script')
  option_parser.add_option('--build-dir', help='ignored')
  option_parser.add_option('--pass-build-dir', action='store_true',
                           default=False,
                           help='pass --build-dir to the spawned test script')
  option_parser.add_option('--test-platform',
                           help='Platform to test on, e.g. ios-simulator')
  option_parser.add_option('--total-shards', dest='total_shards',
                           default=None, type='int',
                           help='Number of shards to split this test into.')
  option_parser.add_option('--shard-index', dest='shard_index',
                           default=None, type='int',
                           help='Shard to run. Must be between 1 and '
                                'total-shards.')
  option_parser.add_option('--run-shell-script', action='store_true',
                           default=False,
                           help='treat first argument as the shell script'
                                'to run.')
  option_parser.add_option('--run-python-script', action='store_true',
                           default=False,
                           help='treat first argument as a python script'
                                'to run.')
  option_parser.add_option('--generate-json-file', action='store_true',
                           default=False,
                           help='output JSON results file if specified.')
  option_parser.add_option('--xvfb', action='store_true', dest='xvfb',
                           default=True,
                           help='Start virtual X server on Linux.')
  option_parser.add_option('--no-xvfb', action='store_false', dest='xvfb',
                           help='Do not start virtual X server on Linux.')
  option_parser.add_option('-o', '--results-directory', default='',
                           help='output results directory for JSON file.')
  option_parser.add_option('--chartjson-file', default='',
                           help='File to dump chartjson results.')
  option_parser.add_option('--log-processor-output-file', default='',
                           help='File to dump gtest log processor results.')
  option_parser.add_option('--builder-name', default=None,
                           help='The name of the builder running this script.')
  option_parser.add_option('--slave-name', default=None,
                           help='The name of the slave running this script.')
  option_parser.add_option('--master-class-name', default=None,
                           help='The class name of the buildbot master running '
                                'this script: examples include "Chromium", '
                                '"ChromiumWebkit", and "ChromiumGPU". The '
                                'flakiness dashboard uses this value to '
                                'categorize results. See buildershandler.py '
                                'in the flakiness dashboard code '
                                '(use codesearch) for the known values. '
                                'Defaults to fetching it from '
                                'slaves.cfg/builders.pyl.')
  option_parser.add_option('--build-number', default=None,
                           help=('The build number of the builder running'
                                 'this script.'))
  option_parser.add_option('--step-name', default=None,
                           help=('The name of the step running this script.'))
  option_parser.add_option('--test-type', default='',
                           help='The test name that identifies the test, '
                                'e.g. \'unit-tests\'')
  option_parser.add_option('--test-results-server', default='',
                           help='The test results server to upload the '
                                'results.')
  option_parser.add_option('--annotate', default='',
                           help='Annotate output when run as a buildstep. '
                                'Specify which type of test to parse, available'
                                ' types listed with --annotate=list.')
  option_parser.add_option('--parse-input', default='',
                           help='When combined with --annotate, reads test '
                                'from a file instead of executing a test '
                                'binary. Use - for stdin.')
  option_parser.add_option('--parse-result', default=0,
                           help='Sets the return value of the simulated '
                                'executable under test. Only has meaning when '
                                '--parse-input is used.')
  option_parser.add_option('--results-url', default='',
                           help='The URI of the perf dashboard to upload '
                                'results to.')
  option_parser.add_option('--perf-dashboard-id', default='',
                           help='The ID on the perf dashboard to add results '
                                'to.')
  option_parser.add_option('--perf-id', default='',
                           help='The perf builder id')
  option_parser.add_option('--perf-config', default='',
                           help='Perf configuration dictionary (as a string). '
                                'This allows to specify custom revisions to be '
                                'the main revision at the Perf dashboard. '
                                'Example: --perf-config="{\'a_default_rev\': '
                                '\'r_webrtc_rev\'}"')
  option_parser.add_option('--supplemental-columns-file',
                           default='supplemental_columns',
                           help='A file containing a JSON blob with a dict '
                                'that will be uploaded to the results '
                                'dashboard as supplemental columns.')
  option_parser.add_option('--revision',
                           help='The revision number which will be is used as '
                                'primary key by the dashboard. If omitted it '
                                'is automatically extracted from the checkout.')
  option_parser.add_option('--webkit-revision',
                           help='See --revision.')
  option_parser.add_option('--enable-asan', action='store_true', default=False,
                           help='Enable fast memory error detection '
                                '(AddressSanitizer).')
  option_parser.add_option('--enable-lsan', action='store_true', default=False,
                           help='Enable memory leak detection (LeakSanitizer).')
  option_parser.add_option('--enable-msan', action='store_true', default=False,
                           help='Enable uninitialized memory reads detection '
                                '(MemorySanitizer).')
  option_parser.add_option('--enable-tsan', action='store_true', default=False,
                           help='Enable data race detection '
                                '(ThreadSanitizer).')
  option_parser.add_option('--strip-path-prefix',
                           default='build/src/out/Release/../../',
                           help='Source paths in stack traces will be stripped '
                           'of prefixes ending with this substring. This '
                           'option is used by sanitizer tools.')
  option_parser.add_option('--no-spawn-dbus', action='store_true',
                           default=False,
                           help='Disable GLib DBus bug workaround: '
                                'manually spawning dbus-launch')
  option_parser.add_option('--test-launcher-summary-output',
                           help='Path to test results file with all the info '
                                'from the test launcher')
  option_parser.add_option('--flakiness-dashboard-server',
                           help='The flakiness dashboard server to which the '
                                'results should be uploaded.')
  option_parser.add_option('--verbose', action='store_true', default=False,
                            help='Prints more information.')

  chromium_utils.AddPropertiesOptions(option_parser)
  options, args = option_parser.parse_args()

  # Initialize logging.
  log_level = logging.INFO
  if options.verbose:
    log_level = logging.DEBUG
  logging.basicConfig(level=log_level,
                      format='%(asctime)s %(filename)s:%(lineno)-3d'
                             ' %(levelname)s %(message)s',
                      datefmt='%y%m%d %H:%M:%S')
  logging.basicConfig(level=logging.DEBUG)
  logging.getLogger().addHandler(logging.StreamHandler(stream=sys.stdout))

  options.test_type = options.test_type or options.factory_properties.get(
      'step_name', '')

  if options.run_shell_script and options.run_python_script:
    sys.stderr.write('Use either --run-shell-script OR --run-python-script, '
                     'not both.')
    return 1

  print '[Running on builder: "%s"]' % options.builder_name

  did_launch_dbus = False
  if not options.no_spawn_dbus:
    did_launch_dbus = _LaunchDBus()

  try:
    options.build_dir = build_directory.GetBuildOutputDirectory()

    if options.pass_target and options.target:
      args.extend(['--target', options.target])
    if options.pass_build_dir:
      args.extend(['--build-dir', options.build_dir])

    # We will use this to accumulate overrides for the command under test,
    # That we may not need or want for other support commands.
    extra_env = {}

    # This option is used by sanitizer code. There is no corresponding command
    # line flag.
    options.use_symbolization_script = False
    # Set up extra environment and args for sanitizer tools.
    _ConfigureSanitizerTools(options, args, extra_env)

    # Set the number of shards environment variables.
    # NOTE: Chromium's test launcher will ignore these in favor of the command
    # line flags passed in _BuildTestBinaryCommand.
    if options.total_shards and options.shard_index:
      extra_env['GTEST_TOTAL_SHARDS'] = str(options.total_shards)
      extra_env['GTEST_SHARD_INDEX'] = str(options.shard_index - 1)

    if options.results_directory:
      options.test_output_xml = os.path.normpath(os.path.abspath(os.path.join(
          options.results_directory, '%s.xml' % options.test_type)))
      args.append('--gtest_output=xml:' + options.test_output_xml)
    elif options.generate_json_file:
      option_parser.error(
          '--results-directory is required with --generate-json-file=True')
      return 1

    return _Main(options, args, extra_env)
  finally:
    if did_launch_dbus:
      # It looks like the command line argument --exit-with-session
      # isn't working to clean up the spawned dbus-daemon. Kill it
      # manually.
      _ShutdownDBus()


if '__main__' == __name__:
  sys.exit(main())
