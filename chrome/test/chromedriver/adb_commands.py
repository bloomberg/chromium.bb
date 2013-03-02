#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A wrapper around adb commands called by chromedriver.

Prerequisites:
  - A single device is attached.
  - adb is in PATH.

This script should write everything (including stacktraces) to stdout.
"""

import optparse
import subprocess
import sys
import traceback


# {<package name>: (<activity name>, <device abstract socket>)}
PACKAGE_INFO = {'org.chromium.chrome.testshell':
                ('org.chromium.chrome.testshell.ChromiumTestShellActivity',
                 'chromium_testshell_devtools_remote'),
                'com.google.android.apps.chrome':
                ('com.google.android.apps.chrome.Main',
                 'chrome_devtools_remote')}


class AdbError(Exception):
  def __init__(self, message, output, cmd):
    self.message = message
    self.output = output
    self.cmd = cmd

  def __str__(self):
    return ('%s\nCommand: "%s"\nOutput: "%s"' %
            (self.message, self.cmd, self.output))


def RunAdbCommand(args, cwd=None):
  """Executes an ADB command and returns its output.

  Args:
    args: A sequence of program arguments supplied to adb.
    cwd: If not None, the subprocess's current directory will be changed to
         |cwd| before it's executed.

  Returns:
    A tuple: (stdout + stderr, command string)

  Raises:
    AdbError: if exit code is non-zero.
  """
  args = ['adb', '-d'] + args
  try:
    p = subprocess.Popen(args, cwd=cwd, stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT)
    stdout, _ = p.communicate()
    args = ' '.join(args)
    if p.returncode:
      raise AdbError('Command failed.', stdout, args)
    return stdout, args
  except OSError as e:
    print 'Make sure adb command is in PATH.'
    raise e


def SetChromeFlags():
  """Sets the command line flags file on device.

  Raises:
    AdbError: If failed to write the flags file to device.
  """
  out, cmd = RunAdbCommand([
      'shell',
      'echo chrome --disable-fre --metrics-recording-only '
      '--enable-remote-debugging > /data/local/chrome-command-line'
      ])
  if out.strip():
    raise AdbError('Failed to set the command line flags.', out, cmd)


def ClearAppData(package):
  """Clears the app data.

  Args:
    package: Application package name.

  Raises:
    AdbError: if any step fails.
  """
  out, cmd = RunAdbCommand(['shell', 'pm clear %s' % package])
  # am/pm package do not return valid exit codes.
  if 'Success' not in out:
    raise AdbError('Failed to clear the profile.', out, cmd)


def LaunchApp(package):
  """Launches the application.

  Args:
    package: Application package name.

  Raises:
    AdbError: if any step fails.
  """
  out, cmd = RunAdbCommand([
      'shell',
      'am start -a android.intent.action.VIEW -S -W -n %s/%s '
      '-d "data:text/html;charset=utf-8,"' %
      (package, PACKAGE_INFO[package][0])])
  if 'Complete' not in out:
    raise AdbError('Failed to start the app. %s', out, cmd)


def Forward(package, host_port):
  """Forward host socket to devtools socket on the device.

  Args:
    package: Application package name.
    host_port: Port on host to forward.

  Raises:
    AdbError: if command fails.
  """
  RunAdbCommand(['forward', 'tcp:%d' % host_port, 'localabstract:%s' %
                 PACKAGE_INFO[package][1]])


if __name__ == '__main__':
  try:
    parser = optparse.OptionParser()
    parser.add_option(
        '', '--package', help='Application package name.')
    parser.add_option(
        '', '--launch', action='store_true',
        help='Launch the app with a fresh profile and forward devtools port.')
    parser.add_option(
        '', '--port', type='int', default=33081,
        help='Host port to forward for launch operation [default: %default].')
    options, _ = parser.parse_args()

    if not options.package:
      raise Exception('No package specified.')

    if options.package not in PACKAGE_INFO:
      raise Exception('Unkown package provided. Supported packages are:\n %s' %
                      PACKAGE_INFO.keys())

    if options.launch:
      SetChromeFlags()
      ClearAppData(options.package)
      LaunchApp(options.package)
      Forward(options.package, options.port)
    else:
      raise Exception('No options provided.')
  except:
    traceback.print_exc(file=sys.stdout)
    sys.exit(1)

