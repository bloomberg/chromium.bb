#!/usr/bin/env python
#
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import json
import optparse
import os
import pipes
import subprocess
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
from pylib import buildbot_report

CHROME_SRC = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..'))

GLOBAL_SLAVE_PROPS = {}

BotConfig = collections.namedtuple(
    'BotConfig', ['bot_id', 'bash_funs', 'test_obj', 'slave_props'])
TestConfig = collections.namedtuple('Tests', ['tests', 'extra_args'])
Command = collections.namedtuple('Command', ['step_name', 'command'])


def GetCommands(options, bot_config):
  """Get a formatted list of commands.

  Args:
    options: Options object.
    bot_config: A BotConfig named tuple.
  Returns:
    list of Command objects.
  """
  slave_props = dict(GLOBAL_SLAVE_PROPS)
  if bot_config.slave_props:
    slave_props.update(bot_config.slave_props)

  property_args = [
      '--factory-properties=%s' % json.dumps(options.factory_properties),
      '--build-properties=%s' % json.dumps(options.build_properties),
      '--slave-properties=%s' % json.dumps(slave_props)]

  commands = []
  if bot_config.bash_funs:
    bash_base = [
        '. build/android/buildbot/buildbot_functions.sh',
        "bb_baseline_setup %s '%s'" %
        (CHROME_SRC, "' '".join(property_args))]
    commands.append(Command(
        None, ['bash', '-exc', ' && '.join(bash_base + bot_config.bash_funs)]))

  test_obj = bot_config.test_obj
  if test_obj:
    run_test_cmd = ['build/android/buildbot/bb_tests.py'] + property_args
    for test in test_obj.tests:
      run_test_cmd.extend(['-f', test])
    if test_obj.extra_args:
      run_test_cmd.extend(test_obj.extra_args)
    commands.append(Command('Run tests', run_test_cmd))

  return commands


def GetBotStepMap():
  compile_step = ['bb_compile']
  std_build_steps = ['bb_compile', 'bb_zip_build']
  std_test_steps = ['bb_extract_build', 'bb_reboot_phones']
  std_tests = ['ui', 'unit']

  B = BotConfig
  def T(tests, extra_args=None):
    return TestConfig(tests, extra_args)

  bot_configs = [
      # Main builders
      B('main-builder-dbg',
        ['bb_compile', 'bb_run_findbugs', 'bb_zip_build'], None, None),
      B('main-builder-rel', compile_step, None, None),
      B('main-clang-builder', compile_step, None, None),
      B('main-clobber', compile_step, None, None),
      B('main-tests-dbg', std_test_steps, T(std_tests), None),

      # Other waterfalls
      B('asan-builder', std_build_steps, None, None),
      B('asan-tests', std_test_steps, T(std_tests, ['--asan']), None),
      B('fyi-builder-dbg',
        ['bb_check_webview_licenses', 'bb_compile', 'bb_compile_experimental',
         'bb_run_findbugs', 'bb_zip_build'], None, None),
      B('fyi-builder-rel',
        ['bb_compile', 'bb_compile_experimental', 'bb_zip_build'], None, None),
      B('fyi-tests', std_test_steps, T(std_tests, ['--experimental']), None),
      B('perf-tests-rel', std_test_steps, T([], ['--install=ContentShell']),
        None),
      B('webkit-latest-tests', std_test_steps, T(['unit']), None),
      B('webkit-latest-webkit-tests', std_test_steps,
        T(['webkit_layout', 'webkit']), None),

      # Generic builder config (for substring match).
      B('builder', std_build_steps, None, None),
  ]

  bot_map = dict((config.bot_id, config) for config in bot_configs)

  # These bots have identical configuration to ones defined earlier.
  copy_map = [
      ('try-builder-dbg', 'main-builder-dbg'),
      ('try-tests-dbg', 'main-tests-dbg'),
      ('try-clang-builder', 'main-clang-builder'),
      ('try-fyi-builder-dbg', 'fyi-builder-dbg'),
      ('try-fyi-tests', 'fyi-tests'),
  ]
  for to_id, from_id in copy_map:
    assert to_id not in bot_map
    # pylint: disable=W0212
    bot_map[to_id] = bot_map[from_id]._replace(bot_id=to_id)

  return bot_map


def main(argv):
  parser = optparse.OptionParser()

  def ConvertJson(option, _, value, parser):
    setattr(parser.values, option.dest, json.loads(value))

  parser.add_option('--build-properties', action='callback',
                    callback=ConvertJson, type='string', default={},
                    help='build properties in JSON format')
  parser.add_option('--factory-properties', action='callback',
                    callback=ConvertJson, type='string', default={},
                    help='factory properties in JSON format')
  parser.add_option('--bot-id', help='Specify bot id directly.')
  parser.add_option('--TESTING', action='store_true',
                    help='For testing: print, but do not run commands')
  options, args = parser.parse_args(argv[1:])
  if args:
    parser.error('Unused args: %s' % args)

  bot_id = options.bot_id or options.factory_properties.get('android_bot_id')
  if not bot_id:
    parser.error('A bot id must be specified through option or factory_props.')

  # Get a BotConfig object looking first for an exact bot-id match. If no exact
  # match, look for a bot-id which is a substring of the specified id.
  # This allows similar bots to have unique IDs, but to share config.
  # If multiple substring matches exist, pick the longest one.
  bot_map = GetBotStepMap()
  bot_config = bot_map.get(bot_id)
  if not bot_config:
    substring_matches = filter(lambda x: x in bot_id, bot_map.iterkeys())
    if substring_matches:
      max_id = max(substring_matches, key=len)
      print 'Using config from id="%s" (substring match).' % max_id
      bot_config = bot_map[max_id]
  if not bot_config:
    print 'Error: config for id="%s" cannot be inferred.' % bot_id
    return 1

  print 'Using config:', bot_config

  def CommandToString(command):
    """Returns quoted command that can be run in bash shell."""
    return ' '.join(map(pipes.quote, command))

  command_objs = GetCommands(options, bot_config)
  for command_obj in command_objs:
    print 'Will run:', CommandToString(command_obj.command)

  return_code = 0
  for command_obj in command_objs:
    if command_obj.step_name:
      buildbot_report.PrintNamedStep(command_obj.step_name)
    command = command_obj.command
    print CommandToString(command)
    sys.stdout.flush()
    env = None
    if options.TESTING:
      # The bash command doesn't yet support the testing option.
      if command[0] == 'bash':
        continue
      env = dict(os.environ)
      env['BUILDBOT_TESTING'] = '1'

    return_code |= subprocess.call(command, cwd=CHROME_SRC, env=env)
  return return_code


if __name__ == '__main__':
  sys.exit(main(sys.argv))
