#!/usr/bin/python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A simple main function to print ui action sequences.

Action sequences are generated using chrome/test/functional/ui_model.py
and are output in the format required by automated_ui_tests build target.

Generate 100 command sequences to ui.txt:
ui_action_generator.py -o ui.txt -c 100

Generate 100 15-action-length sequences:
ui_action_generator.py -c 100 -a 15

Re-create command with seed 12345:
ui_action_generator.py -s 12345
"""

import optparse
import os
import sys
import xml.dom.minidom


def _AddTestPath():
  """Add chrome/test/functional to path to find script dependancies."""
  script_dir = os.path.dirname(__file__)
  chrome_dir = os.path.join(script_dir, os.pardir, os.pardir)
  test_dir = os.path.join(chrome_dir, 'test', 'functional')
  sys.path += [test_dir]

_AddTestPath()
import ui_model


def CreateUIActionList(actions_per_command, num_commands, given_seed=None):
  """Generate user-like pseudo-random action sequences.

  Args:
    actions_per_command: length of each ui action sequence.
    num_commands: number of sequences to generate.
    seed: optional rand seed for this list.

  Returns:
    XML format command list string, readable by automated_ui_tests.
  """
  doc = xml.dom.minidom.Document()
  command_list = doc.createElement('CommandList')
  doc.appendChild(command_list)
  for _ in xrange(num_commands):
    command = doc.createElement('command')
    command_list.appendChild(command)
    seed = ui_model.Seed(given_seed)
    command.setAttribute('seed', str(seed))
    browser = ui_model.BrowserState()
    for _ in xrange(actions_per_command):
      action = ui_model.GetRandomAction(browser)
      browser = ui_model.UpdateState(browser, action)
      action_tuple = action.split(';')
      action_element = doc.createElement(action_tuple[0])
      if len(action_tuple) == 2:
        action_element.setAttribute('url', action_tuple[1])
      command.appendChild(action_element)
  return doc.toprettyxml()


def ParseCommandLine():
  """Parses the command line.

  Returns:
    List of options and their values, and unparsed args.
  """
  parser = optparse.OptionParser()
  parser.add_option('-o', '--output', dest='output_file', type='string',
                    action='store', default='ui_actions.txt',
                    help='the file to output the command list to')
  parser.add_option('-c', '--num_commands', dest='num_commands',
                    type='int', action='store', default=1,
                    help='number of commands to output')
  parser.add_option('-a', '--actions-per-command', dest='actions_per_command',
                    type='int', action='store', default=25,
                    help='number of actions per command')
  parser.add_option('-s', '--seed', dest='seed', type='int', action='store',
                    default=None, help='generate action sequence using a seed')

  return parser.parse_args()


def main():
  """Generate command list and write it out in xml format.

  For use as input for automated_ui_tests build target.
  """
  options, args = ParseCommandLine()
  command_list = CreateUIActionList(options.actions_per_command,
                                    options.num_commands,
                                    options.seed)
  f = open(options.output_file, 'w')
  f.write(command_list)
  f.close()
  print command_list


if __name__ == '__main__':
  main()
