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

import settings
import verifier


class Config:
  """Describes the machine states, actions, and test cases.

  A state is a dictionary where each key is a verifier's name and the
  associated value is the input to that verifier. An action is a shorthand for
  a command. A test is array of alternating state names and action names,
  starting and ending with state names. An instance of this class stores a map
  from state names to state objects, a map from action names to commands, and
  an array of test objects.
  """
  def __init__(self):
    self.states = {}
    self.actions = {}
    self.tests = []


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


def VerifyState(config, state):
  """Verifies that the current machine states match the given machine states.

  Args:
    config: A Config object.
    state: The current state.
  """
  # TODO(sukolsak): Think of ways of preserving the log when the test fails but
  # not printing these when the test passes.
  print settings.PRINT_STATE_PREFIX + state
  verifier.Verify(config.states[state])


def RunCommand(command):
  print settings.PRINT_COMMAND_PREFIX + command
  subprocess.call(command, shell=True)


def RunResetCommand():
  print settings.PRINT_COMMAND_PREFIX + 'Reset'
  # TODO(sukolsak): Need to figure how exactly we want to reset.


def Test(config):
  """Tests the installer using the given Config object.

  Args:
    config: A Config object.
  """
  for test in config.tests:
    print settings.PRINT_TEST_PREFIX + ' -> '.join(test)

    # A Test object is an array of alternating state names and action names.
    # The array starts and ends with states. Therefore, the length must be odd.
    assert(len(test) % 2 == 1)

    RunResetCommand()

    current_state = test[0]
    VerifyState(config, current_state)
    # TODO(sukolsak): Quit the test early if VerifyState fails at any point.

    for i in range(1, len(test), 2):
      action = test[i]
      RunCommand(config.actions[action])

      current_state = test[i + 1]
      VerifyState(config, current_state)


def main():
  parser = argparse.ArgumentParser(description='Test the installer.')
  parser.add_argument('config_filename',
                      help='The relative/absolute path to the config file.')
  args = parser.parse_args()

  config = ParseConfigFile(args.config_filename)
  Test(config)


if __name__ == '__main__':
  main()
