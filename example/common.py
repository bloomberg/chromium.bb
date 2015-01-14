# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import datetime
import getpass
import optparse
import os
import subprocess
import sys


ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.append(os.path.join(ROOT_DIR, '..', 'third_party'))

import colorama


CHROMIUM_SWARMING_OSES = {
    'darwin': 'Mac',
    'cygwin': 'Windows',
    'linux2': 'Linux',
    'win32': 'Windows',
}


def parse_args(use_isolate_server, use_swarming):
  """Process arguments for the example scripts."""
  os.chdir(ROOT_DIR)
  colorama.init()

  parser = optparse.OptionParser(description=sys.modules['__main__'].__doc__)
  if use_isolate_server:
    parser.add_option(
        '-I', '--isolate-server',
        metavar='URL', default=os.environ.get('ISOLATE_SERVER', ''),
        help='Isolate server to use')
  if use_swarming:
    task_name = '%s-%s-hello_world' % (
      getpass.getuser(),
      datetime.datetime.now().strftime('%Y-%m-%d-%H-%M-%S'))
    parser.add_option(
        '--idempotent', action='store_true',
        help='Tells Swarming to reused previous task result if possible')
    parser.add_option(
        '-S', '--swarming',
        metavar='URL', default=os.environ.get('SWARMING_SERVER', ''),
        help='Swarming server to use')
    parser.add_option(
        '-o', '--os', default=sys.platform,
        help='Swarming slave OS to request. Should be one of the valid '
             'sys.platform values like darwin, linux2 or win32 default: '
             '%default.')
    parser.add_option(
        '-t', '--task-name', default=task_name,
        help='Swarming task name, default is based on time: %default')
  parser.add_option('-v', '--verbose', action='count', default=0)
  parser.add_option(
      '--priority', metavar='INT', type='int', help='Priority to use')
  options, args = parser.parse_args()

  if args:
    parser.error('Unsupported argument %s' % args)
  if use_isolate_server and not options.isolate_server:
    parser.error('--isolate-server is required.')
  if use_swarming:
    if not options.swarming:
      parser.error('--swarming is required.')
    options.swarming_os = CHROMIUM_SWARMING_OSES[options.os]
    del options.os

  return options


def note(text):
  """Prints a formatted note."""
  print(
      colorama.Fore.YELLOW + colorama.Style.BRIGHT + '\n-> ' + text +
      colorama.Fore.RESET)


def run(cmd, verbose):
  """Prints the command it runs then run it."""
  cmd = cmd[:]
  cmd.extend(['--verbose'] * verbose)
  print(
      'Running: %s%s%s' %
      (colorama.Fore.GREEN, ' '.join(cmd), colorama.Fore.RESET))
  cmd = [sys.executable, os.path.join('..', cmd[0])] + cmd[1:]
  if sys.platform != 'win32':
    cmd = ['time', '-p'] + cmd
  subprocess.check_call(cmd)


def capture(cmd):
  """Prints the command it runs then return stdout."""
  print(
      'Running: %s%s%s' %
      (colorama.Fore.GREEN, ' '.join(cmd), colorama.Fore.RESET))
  cmd = [sys.executable, os.path.join('..', cmd[0])] + cmd[1:]
  return subprocess.check_output(cmd)
