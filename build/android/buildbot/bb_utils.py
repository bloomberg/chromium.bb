# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import optparse
import os
import pipes
import subprocess
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
from pylib import buildbot_report
from pylib import constants


TESTING = 'BUILDBOT_TESTING' in os.environ

BB_BUILD_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir,
    os.pardir, os.pardir, os.pardir, os.pardir))


def CommandToString(command):
  """Returns quoted command that can be run in bash shell."""
  return ' '.join(map(pipes.quote, command))


def SpawnCmd(command):
  """Spawn a process without waiting for termination."""
  print '>', CommandToString(command)
  sys.stdout.flush()
  if TESTING:
    class MockPopen(object):
      @staticmethod
      def wait():
        return 0
    return MockPopen()

  return subprocess.Popen(command, cwd=constants.DIR_SOURCE_ROOT)


def RunCmd(command, flunk_on_failure=True, halt_on_failure=False,
           warning_code=88):
  """Run a command relative to the chrome source root."""
  code = SpawnCmd(command).wait()
  print '<', CommandToString(command)
  if code != 0:
    print 'ERROR: process exited with code %d' % code
    if code != warning_code and flunk_on_failure:
      buildbot_report.PrintError()
    else:
      buildbot_report.PrintWarning()
    # Allow steps to have both halting (i.e. 1) and non-halting exit codes.
    if code != warning_code and halt_on_failure:
      raise OSError()
  return code


def GetParser():
  def ConvertJson(option, _, value, parser):
    setattr(parser.values, option.dest, json.loads(value))
  parser = optparse.OptionParser()
  parser.add_option('--build-properties', action='callback',
                    callback=ConvertJson, type='string', default={},
                    help='build properties in JSON format')
  parser.add_option('--factory-properties', action='callback',
                    callback=ConvertJson, type='string', default={},
                    help='factory properties in JSON format')
  parser.add_option('--slave-properties', action='callback',
                    callback=ConvertJson, type='string', default={},
                    help='Properties set by slave script in JSON format')

  return parser

