# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script tests the installer with test cases specified in the config file.

For each test case, it checks that the machine states after the execution of
each command match the expected machine states. For more details, take a look at
the design documentation at http://goo.gl/Q0rGM6
"""

import argparse
import datetime
import inspect
import json
import os
import subprocess
import sys
import time
import traceback
import unittest
import _winreg

from variable_expander import VariableExpander
import verifier_runner


def LogMessage(message):
  """Logs a message to stderr.

  Args:
    message: The message string to be logged.
  """
  now = datetime.datetime.now()
  frameinfo = inspect.getframeinfo(inspect.currentframe().f_back)
  filename = os.path.basename(frameinfo.filename)
  line = frameinfo.lineno
  sys.stderr.write('[%s:%s(%s)] %s\n' % (now.strftime('%m%d/%H%M%S'),
                                         filename, line, message))


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

  def __init__(self, name, test, config, variable_expander, quiet):
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
    self._quiet = quiet
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
      if not self._quiet:
        LogMessage('Beginning action %s' % action)
      RunCommand(self._config.actions[action], self._variable_expander)
      if not self._quiet:
        LogMessage('Finished action %s' % action)

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
    if not self._quiet:
      LogMessage('Verifying state %s' % state)
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


def DeleteGoogleUpdateRegistration(system_level, registry_subkey,
                                   variable_expander):
  """Deletes Chrome's registration with Google Update.

  Args:
    system_level: True if system-level Chrome is to be deleted.
    registry_subkey: The pre-expansion registry subkey for the product.
    variable_expander: A VariableExpander object.
  """
  root = (_winreg.HKEY_LOCAL_MACHINE if system_level
          else _winreg.HKEY_CURRENT_USER)
  key_name = variable_expander.Expand(registry_subkey)
  try:
    key_handle = _winreg.OpenKey(root, key_name, 0,
                                 _winreg.KEY_SET_VALUE |
                                 _winreg.KEY_WOW64_32KEY)
    _winreg.DeleteValue(key_handle, 'pv')
  except WindowsError:
    # The key isn't present, so there is no value to delete.
    pass


def RunCleanCommand(force_clean, variable_expander):
  """Puts the machine in the clean state (i.e. Chrome not installed).

  Args:
    force_clean: A boolean indicating whether to force cleaning existing
        installations.
    variable_expander: A VariableExpander object.
  """
  # A list of (system_level, product_name, product_switch, registry_subkey)
  # tuples for the possible installed products.
  data = [
    (False, '$CHROME_LONG_NAME', '',
     '$CHROME_UPDATE_REGISTRY_SUBKEY'),
    (True, '$CHROME_LONG_NAME', '--system-level',
     '$CHROME_UPDATE_REGISTRY_SUBKEY'),
  ]
  if variable_expander.Expand('$SUPPORTS_SXS') == 'True':
    data.append((False, '$CHROME_LONG_NAME_SXS', '',
                 '$CHROME_UPDATE_REGISTRY_SUBKEY_SXS'))

  interactive_option = '--interactive' if not force_clean else ''
  for system_level, product_name, product_switch, registry_subkey in data:
    command = ('python uninstall_chrome.py '
               '--chrome-long-name="%s" '
               '--no-error-if-absent %s %s' %
               (product_name, product_switch, interactive_option))
    try:
      RunCommand(command, variable_expander)
    except:
      message = traceback.format_exception(*sys.exc_info())
      message.insert(0, 'Error cleaning up an old install with:\n')
      LogMessage(''.join(message))
    if force_clean:
      DeleteGoogleUpdateRegistration(system_level, registry_subkey,
                                     variable_expander)


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


def FilterConditionalElem(elem, condition_name, variable_expander):
  """Returns True if a conditional element should be processed.

  Args:
    elem: A dictionary.
    condition_name: The name of the condition property in |elem|.
    variable_expander: A variable expander used to evaluate conditions.

  Returns:
    True if |elem| should be processed.
  """
  if condition_name not in elem:
    return True
  condition = variable_expander.Expand(elem[condition_name])
  return eval(condition, {'__builtins__': {'False': False, 'True': True}})


def ParsePropertyFiles(directory, filenames, variable_expander):
  """Parses an array of .prop files.

  Args:
    directory: The directory where the Config file and all Property files
        reside in.
    filenames: An array of Property filenames.
    variable_expander: A variable expander used to evaluate conditions.

  Returns:
    A property dictionary created by merging all property dictionaries specified
        in the array.
  """
  current_property = {}
  for filename in filenames:
    path = os.path.join(directory, filename)
    new_property = json.load(open(path))
    if not FilterConditionalElem(new_property, 'Condition', variable_expander):
      continue
    # Remove any Condition from the propery dict before merging since it serves
    # no purpose from here on out.
    if 'Condition' in new_property:
      del new_property['Condition']
    MergePropertyDictionaries(current_property, new_property)
  return current_property


def ParseConfigFile(filename, variable_expander):
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
  # Drop conditional tests that should not be run in the current configuration.
  config.tests = filter(lambda t: FilterConditionalElem(t, 'condition',
                                                        variable_expander),
                        config.tests)
  for state_name, state_property_filenames in config_data['states']:
    config.states[state_name] = ParsePropertyFiles(directory,
                                                   state_property_filenames,
                                                   variable_expander)
  for action_name, action_command in config_data['actions']:
    config.actions[action_name] = action_command
  return config


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--build-dir', default='out',
                      help='Path to main build directory (the parent of the '
                      'Release or Debug directory)')
  parser.add_argument('--target', default='Release',
                      help='Build target (Release or Debug)')
  parser.add_argument('--force-clean', action='store_true', default=False,
                      help='Force cleaning existing installations')
  parser.add_argument('-q', '--quiet', action='store_true', default=False,
                      help='Reduce test runner output')
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

  next_version_mini_installer_path = os.path.join(
      args.build_dir, args.target, 'next_version_mini_installer.exe')
  assert os.path.exists(next_version_mini_installer_path), (
      'Could not find file %s' % next_version_mini_installer_path)

  suite = unittest.TestSuite()

  variable_expander = VariableExpander(mini_installer_path,
                                       next_version_mini_installer_path)
  config = ParseConfigFile(args.config, variable_expander)

  RunCleanCommand(args.force_clean, variable_expander)
  for test in config.tests:
    # If tests were specified via |tests|, their names are formatted like so:
    test_name = '%s/%s/%s' % (InstallerTest.__module__,
                              InstallerTest.__name__,
                              test['name'])
    if not args.test or test_name in args.test:
      suite.addTest(InstallerTest(test['name'], test['traversal'], config,
                                  variable_expander, args.quiet))

  verbosity = 2 if not args.quiet else 1
  result = unittest.TextTestRunner(verbosity=verbosity).run(suite)
  if args.write_full_results_to:
    with open(args.write_full_results_to, 'w') as fp:
      json.dump(_FullResults(suite, result, {}), fp, indent=2)
      fp.write('\n')
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
