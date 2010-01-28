# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Breakpad for Python.

Sends a notification when a process stops on an exception."""

import atexit
import getpass
import urllib
import traceback
import socket
import sys


def SendStack(stack, url='http://chromium-status.appspot.com/breakpad'):
  print 'Do you want to send a crash report [y/N]? ',
  if sys.stdin.read(1).lower() != 'y':
    return
  print 'Sending crash report ...'
  try:
    params = {
        'args': sys.argv,
        'stack': stack,
        'user': getpass.getuser(),
    }
    request = urllib.urlopen(url, urllib.urlencode(params))
    print request.read()
    request.close()
  except IOError:
    print('There was a failure while trying to send the stack trace. Too bad.')


def CheckForException():
  last_tb = getattr(sys, 'last_traceback', None)
  if last_tb and sys.last_type is not KeyboardInterrupt:
    SendStack(''.join(traceback.format_tb(last_tb)))


if (not 'test' in sys.modules['__main__'].__file__ and
    socket.gethostname().endswith('.google.com')):
  # Skip unit tests and we don't want anything from non-googler.
  atexit.register(CheckForException)
