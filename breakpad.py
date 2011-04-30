# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Breakpad for Python.

Sends a notification when a process stops on an exception.

It is only enabled when all these conditions are met:
  1. hostname finishes with '.google.com' or 'chromium.org'
  2. main module name doesn't contain the word 'test'
  3. no NO_BREAKPAD environment variable is defined
"""

import atexit
import getpass
import os
import socket
import sys
import time
import traceback
import urllib
import urllib2


# Configure these values.
DEFAULT_URL = 'https://chromium-status.appspot.com'

# Global variable to prevent double registration.
_REGISTERED = False

_TIME_STARTED = time.time()

_HOST_NAME = socket.getfqdn()


def post(url, params):
  """HTTP POST with timeout when it's supported."""
  kwargs = {}
  if (sys.version_info[0] * 10 + sys.version_info[1]) >= 26:
    kwargs['timeout'] = 4
  request = urllib2.urlopen(url, urllib.urlencode(params), **kwargs)
  out = request.read()
  request.close()
  return out


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


def SendStack(last_tb, stack, url=None, maxlen=50):
  """Sends the stack trace to the breakpad server."""
  if not url:
    url = DEFAULT_URL + '/breakpad'
  print 'Sending crash report ...'
  try:
    params = {
        'args': sys.argv,
        'stack': stack[0:4096],
        'user': getpass.getuser(),
        'exception': FormatException(last_tb),
        'host': _HOST_NAME,
        'cwd': os.getcwd(),
        'version': sys.version,
    }
    # pylint: disable=W0702
    print('\n'.join('  %s: %s' % (k, v[0:maxlen])
                    for k, v in params.iteritems()))
    print(post(url, params))
  except IOError:
    print('There was a failure while trying to send the stack trace. Too bad.')


def SendProfiling(url=None):
  try:
    if not url:
      url = DEFAULT_URL + '/profiling'
    params = {
        'argv': ' '.join(sys.argv),
        # Strip the hostname.
        'domain': _HOST_NAME.split('.', 1)[-1],
        'duration': time.time() - _TIME_STARTED,
        'platform': sys.platform,
    }
    post(url, params)
  except IOError:
    pass


def CheckForException():
  """Runs at exit. Look if there was an exception active."""
  last_value = getattr(sys, 'last_value', None)
  if last_value:
    if not isinstance(last_value, KeyboardInterrupt):
      last_tb = getattr(sys, 'last_traceback', None)
      if last_tb:
        SendStack(last_value, ''.join(traceback.format_tb(last_tb)))
  else:
    SendProfiling()


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
    (_HOST_NAME.endswith('.google.com') or
     _HOST_NAME.endswith('.chromium.org'))):
  Register()

# Uncomment this line if you want to test it out.
#Register()
