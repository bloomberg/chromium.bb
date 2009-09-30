#!/usr/bin/python2.4
#
# Copyright 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""A tool to run a chrome sync integration test, used by the buildbot slaves.

  When this is run, the current directory (cwd) should be the outer build
  directory (e.g., chrome-release/build/).

  For a list of command-line options, call this script with '--help'.
"""

__author__ = 'tejasshah@chromium.org'


import logging
import optparse
import os
import re
import subprocess
import sys
import tempfile
import time
import urllib2


USAGE = '%s [options] [test args]' % os.path.basename(sys.argv[0])
HTTP_SERVER_URL = None
HTTP_SERVER_PORT = None


class PathNotFound(Exception): pass


def ListSyncTests(test_exe_path):
  """Returns list of the enabled live sync tests.

  Args:
    test_exe_path: Absolute path to test exe file.

  Returns:
    List of the tests that should be run.
  """
  command = [test_exe_path]
  command.append('--gtest_list_tests')
  proc = subprocess.Popen(
      command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, bufsize=1)
  proc.wait()
  sys.stdout.flush()
  test_list = []
  for line in proc.stdout.readlines():
    if not line.strip():
      continue
    elif line.count("."):  # A new test collection
      test_collection = line.split(".")[0].strip();
    else:                  # An individual test to run
      test_name = line.strip()
      test_list.append("%s.%s" % (test_collection, test_name))
  logging.info('List of tests to run: %s' % (test_list))
  return test_list


def GetUrl(command, port):
  """Prepares the URL with appropriate command to send it to http server.

  Args:
    command: Command for HTTP server
    port: Port number as a parameter to the command

  Returns:
    Formulated URL with the command and parameter.
  """
  command_url = (
      '%s:%s/%s?port=%s'
      % (HTTP_SERVER_URL, HTTP_SERVER_PORT, command, port))
  return command_url


def StartSyncServer(port=None):
  """Starts a chrome sync server.

  Args:
    port: Port number on which sync server to start
      (Default value none, in this case it uses random port).

  Returns:
    If success then port number on which sync server started, else None.
  """
  sync_port = None
  if port:
    start_sync_server_url = GetUrl('startsyncserver', port)
  else:
    start_sync_server_url = (
        '%s:%s/%s' % (HTTP_SERVER_URL, HTTP_SERVER_PORT, 'startsyncserver'))
  req = urllib2.Request(start_sync_server_url)
  try:
    response = urllib2.urlopen(req)
  except urllib2.HTTPError, e:
    logging.error(
        'Could not start sync server, something went wrong.'
        'Request URL: %s , Error Code: %s'% (start_sync_server_url, e.code))
    return sync_port
  except urllib2.URLError, e:
    logging.error(
        'Failed to reach HTTP server.Request URL: %s , Error: %s'
        % (start_sync_server_url, e.reason))
    return sync_port
  else:
    # Let's read response and parse the sync server port number.
    output = response.readlines()
    # Regex to ensure that sync server started and extract the port number.
    regex = re.compile(
        ".*not.*running.*on.*port\s*:\s*(\d+).*started.*new.*sync.*server",
        re.IGNORECASE|re.MULTILINE|re.DOTALL)
    r = regex.search(output[0])
    if r:
      sync_port = r.groups()[0]
      if sync_port:
        logging.info(
            'Started Sync Server Successfully on Port:%s. Request URL: %s , '
            'Response: %s' % (sync_port, start_sync_server_url, output))
    response.fp._sock.recv = None
    response.close()
    return sync_port


def CheckIfSyncServerRunning(port):
  """Check the healthz status of a chrome sync server.

  Args:
    port: Port number on which sync server is running

  Returns:
    True: If sync server running.
    False: Otherwise.
  """
  sync_server_healthz_url = ('%s:%s/healthz' % (HTTP_SERVER_URL, port))
  req = urllib2.Request(sync_server_healthz_url)
  try:
    response = urllib2.urlopen(req)
  except urllib2.HTTPError, e:
    logging.error(
        'It seems like Sync Server is not running, healthz check failed.'
        'Request URL: %s , Error Code: %s'% (sync_server_healthz_url, e.code))
    return False
  except urllib2.URLError, e:
    logging.error(
        'Failed to reach Sync server, healthz check failed.'
        'Request URL: %s , Error: %s'% (sync_server_healthz_url, e.reason))
    return False
  else:
    logging.info(
        'Sync Server healthz check Passed.Request URL: %s , Response: %s'
        % (sync_server_healthz_url, response.readlines()))
    response.fp._sock.recv = None
    response.close()
    return True


def StopSyncServer(port):
  """Stops a chrome sync server.

  Args:
    port: Port number on which sync server to Stop

  Returns:
    Success/Failure as a bool value.
  """
  stop_sync_server_url = GetUrl('stopsyncserver', port)
  req = urllib2.Request(stop_sync_server_url)
  logging.info("Stopping: %s" % stop_sync_server_url)
  try:
    response = urllib2.urlopen(req)
  except urllib2.HTTPError, e:
    logging.error(
        'Could not stop sync server, something went wrong.'
        'Request URL: %s , Error Code: %s'% (stop_sync_server_url, e.code))
    return False
  except urllib2.URLError, e:
    logging.error(
        'Failed to reach HTTP server.Request URL: %s , Error: %s'
        % (stop_sync_server_url, e.reason))
    return False
  else:
    logging.info(
        'Stopped Sync Server Successfully.Request URL: %s , Response: %s'
        % (stop_sync_server_url, response.readlines()))
    response.fp._sock.recv = None
    response.close()
    return True


def ShowGtestLikeFailure(test_exe_path, test_name):
  """Show a gtest-like error when the test can't be run for some reason.

  The scripts responsible for detecting test failures watch for this pattern.

  Args:
    test_exe_path: Absolute path to the test exe file.
    test_name: The name of the test that wasn't run.
  """
  print '[ RUN      ] %s.%s' % (os.path.basename(test_exe_path), test_name)
  print '[  FAILED  ]  %s.%s' % (os.path.basename(test_exe_path), test_name)


def RunCommand(command):
  """Runs the command list, printing its output and returning its exit status.

  Prints the given command (which should be a list of one or more strings),
  then runs it and prints its stdout and stderr together to stdout,
  line-buffered, converting line endings to CRLF (see note below).  Waits for
  the command to terminate and returns its status.

  Args:
    command: Command to run.

  Returns:
    Process exit code.
  """
  print '\n' + subprocess.list2cmdline(command) + '\n',
  proc = subprocess.Popen(command, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, bufsize=1)
  last_flush_time = time.time()
  while proc.poll() == None:
    # Note that Windows Python converts \n to \r\n automatically whenever it
    # encounters it written to a text file (including stdout).  The only way
    # around it is to write to a binary file, which isn't feasible for stdout.
    # So we're stuck with \r\n here even though we explicitly write \n.  (We
    # could write \r instead, which doesn't get converted to \r\n, but that's
    # probably more troublesome for people trying to read the files.)
    line = proc.stdout.readline()
    if line:
      # The comma at the end tells python to not add \n, which is \r\n on
      # Windows.
      print line.rstrip() + '\n',
      # Python on windows writes the buffer only when it reaches 4k. This is
      # not fast enough. Flush each 10 seconds instead.
      if time.time() - last_flush_time >= 10:
        sys.stdout.flush()
        last_flush_time = time.time()
  sys.stdout.flush()
  # Write the remaining buffer.
  for line in proc.stdout.readlines():
    print line.rstrip() + '\n',
  sys.stdout.flush()
  return proc.returncode


def RunOneTest(test_exe_path, test_name, username, password):
  """Run one live sync test after setting up a fresh server environment.

  Args:
    test_exe_path: Absolute path to test exe file.
    test_name: the name of the one test to run.
    username: test account username.
    password: test account password.

  Returns:
    Zero for suceess.
    Non-zero for failure.
  """
  def LogTestNotRun(message):
    ShowGtestLikeFailure(test_exe_path, test_name)
    logging.info('\n\n%s did not run because %s' % (test_name, message))

  try:
    logging.info('\n\n*************************************')
    logging.info('%s Start' % (test_name))
    sync_port = StartSyncServer()
    if not sync_port:
      LogTestNotRun('starting the sync server failed.')
      return 1
    if not CheckIfSyncServerRunning(sync_port):
      LogTestNotRun('sync server running check failed.')
      return 1
    logging.info('Verified that sync server is running on port: %s', sync_port)
    user_dir = GenericSetup(test_name)
    logging.info('Created user data dir %s' % (user_dir))
    command = [
        test_exe_path,
        '--gtest_filter='+ test_name,
        '--user-data-dir=' + user_dir,
        '--sync-user-for-test=' + username,
        '--sync-password-for-test=' + password,
        '--sync-url=' + HTTP_SERVER_URL + ':' + sync_port]
    logging.info(
        '%s will run with command: %s'
        % (test_name, subprocess.list2cmdline(command)))
    result = RunCommand(command)
    StopSyncServer(sync_port)
    return result
  finally:
    logging.info('%s End' % (test_name))


def GenericSetup(test_name):
  """Generic setup for running one test.

  Args:
    test_name: The name of a test that is about to be run.

  Returns:
    user_dir: Absolute path to user data dir created for the test.
  """
  user_dir = tempfile.mkdtemp(prefix=test_name + '.')
  return user_dir


def main_win(options, args):
  """Main Function for win32 platform which drives the test here.

  Using the target build configuration, run the executable given in the
  first non-option argument, passing any following arguments to that
  executable.

  Args:
    options: Option parameters.
    args: Test arguments.

  Returns:
    Result: Zero for success/ Non-zero for failure.
  """
  final_result = 0
  test_exe = 'sync_integration_tests.exe'
  build_dir = os.path.abspath(options.build_dir)
  test_exe_path = os.path.join(build_dir, options.target, test_exe)
  if not os.path.exists(test_exe_path):
    raise PathNotFound('Unable to find %s' % test_exe_path)
  test_list = ListSyncTests(test_exe_path)
  for test_name in test_list:
    result = RunOneTest(
        test_exe_path, test_name, options.sync_test_username,
        options.sync_test_password)
    # If any single test fails then final result should be failure
    if result != 0:
      final_result = result
  return final_result

if '__main__' == __name__:
  # Initialize logging.
  log_level = logging.INFO
  logging.basicConfig(
      level=log_level, format='%(asctime)s %(filename)s:%(lineno)-3d'
      ' %(levelname)s %(message)s', datefmt='%y%m%d %H:%M:%S')

  option_parser = optparse.OptionParser(usage=USAGE)

  # Since the trailing program to run may have has command-line args of its
  # own, we need to stop parsing when we reach the first positional argument.
  option_parser.disable_interspersed_args()

  option_parser.add_option(
      '', '--target', default='Release', help='Build target (Debug or Release)'
      ' Default value Release.')
  option_parser.add_option(
      '', '--build-dir', help='Path to main build directory'
      '(the parent of the Release or Debug directory).')
  option_parser.add_option(
      '', '--http-server-url', help='Path to http server that can be used to'
      ' start/stop sync server e.g. http://http_server_url_without_port')
  option_parser.add_option(
      '', '--http-server-port', help='Port on which http server is running'
      ' e.g. 1010')
  option_parser.add_option(
      '', '--sync-test-username', help='Test username e.g. foo@gmail.com')
  option_parser.add_option(
      '', '--sync-test-password', help='Password for the test account')
  options, args = option_parser.parse_args()
  if not options.sync_test_password:
    # Check along side this script under src/ first, and then check in
    # the user profile dir.
    password_file_path = os.path.join(
        os.path.dirname(__file__),'sync_password')
    if (not os.path.exists(password_file_path)):
      password_file_path = os.path.join(
          os.environ['USERPROFILE'], 'sync_password')

    if os.path.exists(password_file_path):
      fs = open(password_file_path, 'r')
      lines = fs.readlines()
      if len(lines)==1:
        options.sync_test_password = lines[0].strip()
      else:
        sys.stderr.write('sync_password file is not in required format.\n')
        sys.exit(1)
    else:
      sys.stderr.write(
          'Missing required parameter- sync_test_password, please fix it.\n')
      sys.exit(1)
  if (not options.build_dir or not options.http_server_url or
      not options.http_server_port or not options.sync_test_username):
    sys.stderr.write('Missing required parameter, please fix it.\n')
    option_parser.print_help()
    sys.exit(1)
  if sys.platform == 'win32':
    HTTP_SERVER_URL = options.http_server_url
    HTTP_SERVER_PORT = options.http_server_port
    sys.exit(main_win(options, args))
  else:
    sys.stderr.write('Unknown sys.platform value %s\n' % repr(sys.platform))
    sys.exit(1)
