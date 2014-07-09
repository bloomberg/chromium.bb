# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script tests the installer with test cases specified in the config file.

For each test case, it checks that the machine states after the execution of
each command match the expected machine states. For more details, take a look at
the design documentation at http://goo.gl/Q0rGM6
"""

import json
import optparse
import os
import subprocess
import sys
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

  def __init__(self, test, config, variable_expander):
    """Constructor.

    Args:
      test: An array of alternating state names and action names, starting and
          ending with state names.
      config: The Config object.
      variable_expander: A VariableExpander object.
    """
    super(InstallerTest, self).__init__()
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
    return 'Test: %s\n' % (' -> '.join(self._test))

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
  config_data = json.load(open(filename, 'r'))
  directory = os.path.dirname(os.path.abspath(filename))

  config = Config()
  config.tests = config_data['tests']
  for state_name, state_property_filenames in config_data['states']:
    config.states[state_name] = ParsePropertyFiles(directory,
                                                   state_property_filenames)
  for action_name, action_command in config_data['actions']:
    config.actions[action_name] = action_command
  return config


def RunTests(mini_installer_path, config, force_clean):
  """Tests the installer using the given Config object.

  Args:
    mini_installer_path: The path to mini_installer.exe.
    config: A Config object.
    force_clean: A boolean indicating whether to force cleaning existing
        installations.

  Returns:
    True if all the tests passed, or False otherwise.
  """
  suite = unittest.TestSuite()
  variable_expander = VariableExpander(mini_installer_path)
  RunCleanCommand(force_clean, variable_expander)
  for test in config.tests:
    suite.addTest(InstallerTest(test, config, variable_expander))
  result = unittest.TextTestRunner(verbosity=2).run(suite)
  return result.wasSuccessful()


def IsComponentBuild(mini_installer_path):
  """ Invokes the mini_installer asking whether it is a component build.

  Args:
    mini_installer_path: The path to mini_installer.exe.

  Returns:
    True if the mini_installer is a component build, False otherwise.
  """
  query_command = mini_installer_path + ' --query-component-build'
  script_dir = os.path.dirname(os.path.abspath(__file__))
  exit_status = subprocess.call(query_command, shell=True, cwd=script_dir)
  return exit_status == 0


def main():
  usage = 'usage: %prog [options] config_filename'
  parser = optparse.OptionParser(usage, description='Test the installer.')
  parser.add_option('--build-dir', default='out',
                    help='Path to main build directory (the parent of the '
                         'Release or Debug directory)')
  parser.add_option('--target', default='Release',
                    help='Build target (Release or Debug)')
  parser.add_option('--force-clean', action='store_true', dest='force_clean',
                    default=False, help='Force cleaning existing installations')
  options, args = parser.parse_args()
  if len(args) != 1:
    parser.error('Incorrect number of arguments.')
  config_filename = args[0]

  mini_installer_path = os.path.join(options.build_dir, options.target,
                                     'mini_installer.exe')
  assert os.path.exists(mini_installer_path), ('Could not find file %s' %
                                               mini_installer_path)

  # Set the env var used by mini_installer.exe to decide to not show UI.
  os.environ['MINI_INSTALLER_TEST'] = '1'
  if IsComponentBuild(mini_installer_path):
    print ('Component build is currently unsupported by the mini_installer: '
           'http://crbug.com/377839')
    return 0

  config = ParseConfigFile(config_filename)
  if not RunTests(mini_installer_path, config, options.force_clean):
    return 1
  return 0


if __name__ == '__main__':
  sys.exit(main())
