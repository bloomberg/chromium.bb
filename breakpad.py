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


# Configure these values.
DEFAULT_URL = 'https://chromium-status.appspot.com/breakpad'

_REGISTERED = False


def SendStack(last_tb, stack, url=None):
  """Sends the stack trace to the breakpad server."""
  if not url:
    url = DEFAULT_URL
  print 'Sending crash report ...'
  try:
    params = {
        'args': sys.argv,
        'stack': stack,
        'user': getpass.getuser(),
        'exception': last_tb,
        'host': socket.getfqdn(),
    }
    request = urllib.urlopen(url, urllib.urlencode(params))
    print request.read()
    request.close()
  except IOError:
    print('There was a failure while trying to send the stack trace. Too bad.')


def CheckForException():
  """Runs at exit. Look if there was an exception active."""
  last_value = getattr(sys, 'last_value', None)
  if last_value and not isinstance(last_value, KeyboardInterrupt):
    last_tb = getattr(sys, 'last_traceback', None)
    if last_tb:
      SendStack(repr(last_value), ''.join(traceback.format_tb(last_tb)))


def Register():
  """Registers the callback at exit. Calling it multiple times is no-op."""
  global _REGISTERED
  if _REGISTERED:
    return
  _REGISTERED = True
  atexit.register(CheckForException)


# Skip unit tests and we don't want anything from non-googler.
if (not 'test' in sys.modules['__main__'].__file__ and
    socket.getfqdn().endswith('.google.com')):
  Register()

# Uncomment this line if you want to test it out.
#Register()
