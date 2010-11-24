# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Breakpad for Python.

Sends a notification when a process stops on an exception.

It is only enabled when all these conditions are met:
  1. hostname finishes with '.google.com'
  2. main module name doesn't contain the word 'test'
  3. no NO_BREAKPAD environment variable is defined
"""

import atexit
import getpass
import os
import urllib
import traceback
import socket
import sys


# Configure these values.
DEFAULT_URL = 'https://chromium-status.appspot.com/breakpad'

_REGISTERED = False


def FormatException(e):
  """Returns a human readable form of an exception.

  Adds the maximum number of interesting information in the safest way."""
  try:
    out = repr(e)
  except Exception:
    out = ''
  try:
    out = str(e)
    if isinstance(e, Exception):
      # urllib exceptions, usually the HTTP headers.
      if hasattr(e, 'headers'):
        out += '\nHeaders: %s' % e.headers
      if hasattr(e, 'url'):
        out += '\nUrl: %s' % e.url
      if hasattr(e, 'msg'):
        out += '\nMsg: %s' % e.msg
      # The web page in some urllib exceptions.
      if hasattr(e, 'read') and callable(e.read):
        out += '\nread(): %s' % e.read()
      if hasattr(e, 'info') and callable(e.info):
        out += '\ninfo(): %s' % e.info()
  except Exception:
    pass
  return out


def SendStack(last_tb, stack, url=None):
  """Sends the stack trace to the breakpad server."""
  if not url:
    url = DEFAULT_URL
  print 'Sending crash report ...'
  try:
    params = {
        'args': sys.argv,
        'stack': stack[0:4096],
        'user': getpass.getuser(),
        'exception': FormatException(last_tb),
        'host': socket.getfqdn(),
        'cwd': os.getcwd(),
    }
    # pylint: disable=W0702
    print('\n'.join('  %s: %s' % (k, v[0:50]) for k, v in params.iteritems()))
    request = urllib.urlopen(url, urllib.urlencode(params))
    print(request.read())
    request.close()
  except IOError:
    print('There was a failure while trying to send the stack trace. Too bad.')


def CheckForException():
  """Runs at exit. Look if there was an exception active."""
  last_value = getattr(sys, 'last_value', None)
  if last_value and not isinstance(last_value, KeyboardInterrupt):
    last_tb = getattr(sys, 'last_traceback', None)
    if last_tb:
      SendStack(last_value, ''.join(traceback.format_tb(last_tb)))


def Register():
  """Registers the callback at exit. Calling it multiple times is no-op."""
  global _REGISTERED
  if _REGISTERED:
    return
  _REGISTERED = True
  atexit.register(CheckForException)


# Skip unit tests and we don't want anything from non-googler.
if (not 'test' in sys.modules['__main__'].__file__ and
    not 'NO_BREAKPAD' in os.environ and
    (socket.getfqdn().endswith('.google.com') or
     socket.getfqdn().endswith('.chromium.org'))):
  Register()

# Uncomment this line if you want to test it out.
#Register()
