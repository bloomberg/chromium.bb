#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Launch a local server on an ephemeral port, then launch a executable that
points to that server.
"""

import copy
import getos
import optparse
import os
import subprocess
import sys
import httpd


if sys.version_info < (2, 6, 0):
  sys.stderr.write("python 2.6 or later is required run this script\n")
  sys.exit(1)


def main(args):
  usage = """usage: %prog [options] -- executable args...

  This command creates a local server on an ephemeral port, then runs:
    <executable> <args..> http://localhost:<port>/<page>.

  Where <page> can be set by -P, or uses index.html by default."""
  parser = optparse.OptionParser(usage)
  parser.add_option('-C', '--serve-dir',
      help='Serve files out of this directory.',
      dest='serve_dir', default=os.path.abspath('.'))
  parser.add_option('-P', '--path', help='Path to load from local server.',
      dest='path', default='index.html')
  parser.add_option('-D',
      help='Add debug command-line when launching the chrome debug.',
      dest='debug', action='append', default=[])
  parser.add_option('-E',
      help='Add environment variables when launching the executable.',
      dest='environ', action='append', default=[])
  parser.add_option('--test-mode',
      help='Listen for posts to /ok or /fail and shut down the server with '
          ' errorcodes 0 and 1 respectively.',
      dest='test_mode', action='store_true')
  parser.add_option('-p', '--port',
      help='Port to run server on. Default is 5103, ephemeral is 0.',
      default=5103)
  options, args = parser.parse_args(args)
  if not args:
    parser.error('No executable given.')

  # 0 means use an ephemeral port.
  server = httpd.LocalHTTPServer(options.serve_dir, options.port,
                                 options.test_mode)
  print 'Serving %s on %s...' % (options.serve_dir, server.GetURL(''))

  env = copy.copy(os.environ)
  for e in options.environ:
    key, value = map(str.strip, e.split('='))
    env[key] = value

  cmd = args + [server.GetURL(options.path)]
  print 'Running: %s...' % (' '.join(cmd),)
  process = subprocess.Popen(cmd, env=env)

  # If any debug args are passed in, assume we want to debug
  if options.debug:
    if getos.GetPlatform() != 'win':
      cmd = ['xterm', '-title', 'NaCl Debugger', '-e']
    else:
      cmd = []
    cmd += options.debug
    print 'Starting debugger: ' + ' '.join(cmd)
    debug_process = subprocess.Popen(cmd, env=env)
  else:
    debug_process = False

  try:
    return server.ServeUntilSubprocessDies(process)
  finally:
    if process.returncode is None:
      process.kill()
    if debug_process and debug_process.returncode is None:
      debug_process.kill()

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
