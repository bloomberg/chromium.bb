# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

import gdb_rsp


def TestContinue(connection):
  result = connection.RspRequest('vCont;c')
  # Once the NaCl test module reports that the test passed, the NaCl <embed>
  # element is removed from the page and so the NaCl module is killed by
  # the browser what is reported as exit due to SIGKILL (X09).
  assert result == 'X09', result


def Main(args):
  port = int(args[0])
  name = args[1]
  connection = gdb_rsp.GdbRspConnection(('localhost', port))
  if name == 'continue':
    TestContinue(connection)
  else:
    raise AssertionError('Unknown test name: %r' % name)


if __name__ == '__main__':
  Main(sys.argv[1:])