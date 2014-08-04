# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script tests the installer with test cases specified in the config file.

For each test case, it checks that the machine states after the execution of
each command match the expected machine states. For more details, take a look at
the design documentation at http://goo.gl/Q0rGM6
"""

import argparse
import json
import os
import subprocess
import sys
import time
import unittest

from variable_expander import VariableExpander
import verifier_runner


class Config:
  """Describes the machine states, actions, and test cases.

  Attributes:
    states: A dictionary where each key is a state name and the associated value
        is a property dictionary describing that state.
    actions: A dictionary where each key is an action name and the associated
        value is the action's command.
    tests: An array of test cases.
  """
  def __init__(self):
    self.states = {}
    self.actions = {}
    self.tests = []


class InstallerTest(unittest.TestCase):
  """Tests a test case in the config file."""

  def __init__(self, name, test, config, variable_expander):
    """Constructor.

    Args:
      name: The name of this test.
      test: An array of alternating state names and action names, starting and
          ending with state names.
      config: The Config object.
      variable_expander: A VariableExpander object.
    """
    super(InstallerTest, self).__init__()
    self._name = name
    self._test = test
    self._config = config
    self._variable_expander = variable_expander
    self._verifier_runner = verifier_runner.VerifierRunner()
    self._clean_on_teardown = True

  def __str__(self):
    """Returns a string representing the test case.

    Returns:
      A string created by joining state names and action names together with
      ' -> ', for example, 'Test: clean -> install chrome -> chrome_installed'.
    """
    return '%s: %s\n' % (self._name, ' -> '.join(self._test))

  def id(self):
    """Returns the name of the test."""
    # Overridden from unittest.TestCase so that id() contains the name of the
    # test case from the config file in place of the name of this class's test
    # function.
    return unittest.TestCase.id(self).replace(self._testMethodName, self._name)

  def runTest(self):
    """Run the test case."""
    # |test| is an array of alternating state names and action names, starting
    # and ending with state names. Therefore, its length must be odd.
    self.assertEqual(1, len(self._test) % 2,
                     'The length of test array must be odd')

    state = self._test[0]
    self._VerifyState(state)

    # Starting at index 1, we loop through pairs of (action, state).
    for i in range(1, len(self._test), 2):
      action = self._test[i]
      RunCommand(self._config.actions[action], self._variable_expander)

      state = self._test[i + 1]
      self._VerifyState(state)

    # If the test makes it here, it means it was successful, because RunCommand
    # and _VerifyState throw an exception on failure.
    self._clean_on_teardown = False

  def tearDown(self):
    """Cleans up the machine if the test case fails."""
    if self._clean_on_teardown:
      RunCleanCommand(True, self._variable_expander)

  def shortDescription(self):
    """Overridden from unittest.TestCase.

    We return None as the short description to suppress its printing.
    The default implementation of this method returns the docstring of the
    runTest method, which is not useful since it's the same for every test case.
    The description from the __str__ method is informative enough.
    """
    return None

  def _VerifyState(self, state):
    """Verifies that the current machine state matches a given state.

    Args:
      state: A state name.
    """
    try:
      self._verifier_runner.VerifyAll(self._config.states[state],
                                      self._variable_expander)
    except AssertionError as e:
      # If an AssertionError occurs, we intercept it and add the state name
      # to the error message so that we know where the test fails.
      raise AssertionError("In state '%s', %s" % (state, e))


def RunCommand(command, variable_expander):
  """Runs the given command from the current file's directory.

  This function throws an Exception if the command returns with non-zero exit
  status.

  Args:
    command: A command to run. It is expanded using Expand.
    variable_expander: A VariableExpander object.
  """
  expanded_command = variable_expander.Expand(command)
  script_dir = os.path.dirname(os.path.abspath(__file__))
  exit_status = subprocess.call(expanded_command, shell=True, cwd=script_dir)
  if exit_status != 0:
    raise Exception('Command %s returned non-zero exit status %s' % (
        expanded_command, exit_status))


def RunCleanCommand(force_clean, variable_expander):
  """Puts the machine in the clean state (i.e. Chrome not installed).

  Args:
    force_clean: A boolean indicating whether to force cleaning existing
        installations.
    variable_expander: A VariableExpander object.
  """
  # TODO(sukolsak): Read the clean state from the config file and clean
  # the machine according to it.
  # TODO(sukolsak): Handle Chrome SxS installs.
  commands = []
  interactive_option = '--interactive' if not force_clean else ''
  for level_option in ('', '--system-level'):
    commands.append('python uninstall_chrome.py '
                    '--chrome-long-name="$CHROME_LONG_NAME" '
                    '--no-error-if-absent %s %s' %
                    (level_option, interactive_option))
  RunCommand(' && '.join(commands), variable_expander)


def MergePropertyDictionaries(current_property, new_property):
  """Merges the new property dictionary into the current property dictionary.

  This is different from general dictionary merging in that, in case there are
  keys with the same name, we merge values together in the first level, and we
  override earlier values in the second level. For more details, take a look at
  http://goo.gl/uE0RoR

  Args:
    current_property: The property dictionary to be modified.
    new_property: The new property dictionary.
  """
  for key, value in new_property.iteritems():
    if key not in current_property:
      current_property[key] = value
    else:
      assert(isinstance(current_property[key], dict) and
          isinstance(value, dict))
      # This merges two dictionaries together. In case there are keys with
      # the same name, the latter will override the former.
      current_property[key] = dict(
          current_property[key].items() + value.items())


def ParsePropertyFiles(directory, filenames):
  """Parses an array of .prop files.

  Args:
    property_filenames: An array of Property filenames.
    directory: The directory where the Config file and all Property files
        reside in.

  Returns:
    A property dictionary created by merging all property dictionaries specified
        in the array.
  """
  current_property = {}
  for filename in filenames:
    path = os.path.join(directory, filename)
    new_property = json.load(open(path))
    MergePropertyDictionaries(current_property, new_property)
  return current_property


def ParseConfigFile(filename):
  """Parses a .config file.

  Args:
    config_filename: A Config filename.

  Returns:
    A Config object.
  """
  with open(filename, 'r') as fp:
    config_data = json.load(fp)
  directory = os.path.dirname(os.path.abspath(filename))

  config = Config()
  config.tests = config_data['tests']
  for state_name, state_property_filenames in config_data['states']:
    config.states[state_name] = ParsePropertyFiles(directory,
                                                   state_property_filenames)
  for action_name, action_command in config_data['actions']:
    config.actions[action_name] = action_command
  return config


def IsComponentBuild(mini_installer_path):
  """ Invokes the mini_installer asking whether it is a component build.

  Args:
    mini_installer_path: The path to mini_installer.exe.

  Returns:
    True if the mini_installer is a component build, False otherwise.
  """
  query_command = [ mini_installer_path, '--query-component-build' ]
  exit_status = subprocess.call(query_command)
  return exit_status == 0


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--build-dir', default='out',
                      help='Path to main build directory (the parent of the '
                      'Release or Debug directory)')
  parser.add_argument('--target', default='Release',
                      help='Build target (Release or Debug)')
  parser.add_argument('--force-clean', action='store_true', default=False,
                      help='Force cleaning existing installations')
  parser.add_argument('-v', '--verbose', action='count', default=0,
                      help='Increase test runner verbosity level')
  parser.add_argument('--write-full-results-to', metavar='FILENAME',
                      help='Path to write the list of full results to.')
  parser.add_argument('--config', metavar='FILENAME',
                      help='Path to test configuration file')
  parser.add_argument('test', nargs='*',
                      help='Name(s) of tests to run.')
  args = parser.parse_args()
  if not args.config:
    parser.error('missing mandatory --config FILENAME argument')

  mini_installer_path = os.path.join(args.build_dir, args.target,
                                     'mini_installer.exe')
  assert os.path.exists(mini_installer_path), ('Could not find file %s' %
                                               mini_installer_path)

  suite = unittest.TestSuite()

  # Set the env var used by mini_installer.exe to decide to not show UI.
  os.environ['MINI_INSTALLER_TEST'] = '1'
  is_component_build = IsComponentBuild(mini_installer_path)
  if not is_component_build:
    config = ParseConfigFile(args.config)

    variable_expander = VariableExpander(mini_installer_path)
    RunCleanCommand(args.force_clean, variable_expander)
    for test in config.tests:
      # If tests were specified via |tests|, their names are formatted like so:
      test_name = '%s.%s.%s' % (InstallerTest.__module__,
                                InstallerTest.__name__,
                                test['name'])
      if not args.test or test_name in args.test:
        suite.addTest(InstallerTest(test['name'], test['traversal'], config,
                                    variable_expander))

  result = unittest.TextTestRunner(verbosity=(args.verbose + 1)).run(suite)
  if is_component_build:
    print ('Component build is currently unsupported by the mini_installer: '
           'http://crbug.com/377839')
  if args.write_full_results_to:
    with open(args.write_full_results_to, 'w') as fp:
      json.dump(_FullResults(suite, result, {}), fp, indent=2)
      fp.write("\n")
  return 0 if result.wasSuccessful() else 1


# TODO(dpranke): Find a way for this to be shared with the mojo and other tests.
TEST_SEPARATOR = '.'


def _FullResults(suite, result, metadata):
  """Convert the unittest results to the Chromium JSON test result format.

  This matches run-webkit-tests (the layout tests) and the flakiness dashboard.
  """

  full_results = {}
  full_results['interrupted'] = False
  full_results['path_delimiter'] = TEST_SEPARATOR
  full_results['version'] = 3
  full_results['seconds_since_epoch'] = time.time()
  for md in metadata:
    key, val = md.split('=', 1)
    full_results[key] = val

  all_test_names = _AllTestNames(suite)
  failed_test_names = _FailedTestNames(result)

  full_results['num_failures_by_type'] = {
      'FAIL': len(failed_test_names),
      'PASS': len(all_test_names) - len(failed_test_names),
  }

  full_results['tests'] = {}

  for test_name in all_test_names:
    value = {}
    value['expected'] = 'PASS'
    if test_name in failed_test_names:
      value['actual'] = 'FAIL'
      value['is_unexpected'] = True
    else:
      value['actual'] = 'PASS'
    _AddPathToTrie(full_results['tests'], test_name, value)

  return full_results


def _AllTestNames(suite):
  test_names = []
  # _tests is protected  pylint: disable=W0212
  for test in suite._tests:
    if isinstance(test, unittest.suite.TestSuite):
      test_names.extend(_AllTestNames(test))
    else:
      test_names.append(test.id())
  return test_names


def _FailedTestNames(result):
  return set(test.id() for test, _ in result.failures + result.errors)


def _AddPathToTrie(trie, path, value):
  if TEST_SEPARATOR not in path:
    trie[path] = value
    return
  directory, rest = path.split(TEST_SEPARATOR, 1)
  if directory not in trie:
    trie[directory] = {}
  _AddPathToTrie(trie[directory], rest, value)


if __name__ == '__main__':
  sys.exit(main())
