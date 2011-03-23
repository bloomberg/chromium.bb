#!/usr/bin/env python
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Backport of subprocess.check_output from python2.7
#  http://svn.python.org/view/*checkout*/python/tags/r271/Lib/subprocess.py
#
# Headers for that file are:
# subprocess - Subprocesses with accessible I/O streams
#
# For more information about this module, see PEP 324.
#
# This module should remain compatible with Python 2.2, see PEP 291.
#
# Copyright (c) 2003-2005 by Peter Astrand <astrand@lysator.liu.se>
#
# Licensed to PSF under a Contributor Agreement.
# See http://www.python.org/2.4/license for licensing details.

import subprocess
import unittest


def check_output(*popenargs, **kwargs):
    r"""Run command with arguments and return its output as a byte string.

    If the exit code was non-zero it raises a CalledProcessError.  The
    CalledProcessError object will have the return code in the returncode
    attribute and output in the output attribute.

    The arguments are the same as for the Popen constructor.  Example:

    >>> check_output(["ls", "-l", "/dev/null"])
    'crw-rw-rw- 1 root root 1, 3 Oct 18  2007 /dev/null\n'

    The stdout argument is not allowed as it is used internally.
    To capture standard error in the result, use stderr=STDOUT.

    >>> check_output(["/bin/sh", "-c",
    ...               "ls -l non_existent_file ; exit 0"],
    ...              stderr=STDOUT)
    'ls: non_existent_file: No such file or directory\n'
    """
    if 'stdout' in kwargs:
        raise ValueError('stdout argument not allowed, it will be overridden.')
    process = subprocess.Popen(stdout=subprocess.PIPE, *popenargs, **kwargs)
    output, unused_err = process.communicate()
    retcode = process.poll()
    if retcode:
        cmd = kwargs.get("args")
        if cmd is None:
            cmd = popenargs[0]
        raise subprocess.CalledProcessError(retcode, cmd)
    return output


class CheckOutputTester(unittest.TestCase):
  def test_check_output(self):
    self.assertRaises(subprocess.CalledProcessError,
                      check_output, '/bin/false')

    self.assertRaises(ValueError,
                      check_output, ['/bin/true'], stdout='/tmp/nonexistant')

    self.assertEqual('hello\n',
                     check_output(['/bin/echo', 'hello']))

    echoer = subprocess.Popen(['/bin/echo', 'hello'], stdout=subprocess.PIPE)
    self.assertEqual('hello\n',
                     check_output(['/bin/cat'], stdin=echoer.stdout))

if __name__ == '__main__':
  unittest.main()
