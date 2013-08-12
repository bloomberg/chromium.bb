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
import unittest

import verifier


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

  def __init__(self, test, config):
    """Constructor.

    Args:
      test: An array of alternating state names and action names, starting and
          ending with state names.
      config: The Config object.
    """
    super(InstallerTest, self).__init__()
    self._test = test
    self._config = config

  def __str__(self):
    """Returns a string representing the test case.

    Returns:
      A string created by joining state names and action names together with
      ' -> ', for example, 'Test: clean -> install chrome -> chrome_installed'.
    """
    return 'Test: %s' % (' -> '.join(self._test))

  def runTest(self):
    """Run the test case."""
    # |test| is an array of alternating state names and action names, starting
    # and ending with state names. Therefore, its length must be odd.
    self.assertEqual(1, len(self._test) % 2,
                     'The length of test array must be odd')

    # TODO(sukolsak): run a reset command that puts the machine in clean state.

    state = self._test[0]
    self._VerifyState(state)

    # Starting at index 1, we loop through pairs of (action, state).
    for i in range(1, len(self._test), 2):
      action = self._test[i]
      self._RunCommand(self._config.actions[action])

      state = self._test[i + 1]
      self._VerifyState(state)

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
      verifier.Verify(self._config.states[state])
    except AssertionError as e:
      # If an AssertionError occurs, we intercept it and add the state name
      # to the error message so that we know where the test fails.
      raise AssertionError("In state '%s', %s" % (state, e))

  def _RunCommand(self, command):
    subprocess.call(command, shell=True)


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


def RunTests(config):
  """Tests the installer using the given Config object.

  Args:
    config: A Config object.
  """
  suite = unittest.TestSuite()
  for test in config.tests:
    suite.addTest(InstallerTest(test, config))
  unittest.TextTestRunner(verbosity=2).run(suite)


def main():
  parser = argparse.ArgumentParser(description='Test the installer.')
  parser.add_argument('config_filename',
                      help='The relative/absolute path to the config file.')
  args = parser.parse_args()

  config = ParseConfigFile(args.config_filename)
  RunTests(config)


if __name__ == '__main__':
  main()
