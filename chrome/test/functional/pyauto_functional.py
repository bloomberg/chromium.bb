#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Setup for PyAuto functional tests.

Use the following in your scripts to run them standalone:

# This should be at the top
import pyauto_functional

if __name__ == '__main__':
  pyauto_functional.Main()

This script can be used as an executable to fire off other scripts, similar
to unittest.py
  python pyauto_functional.py test_script
"""

import os
import subprocess
import sys


def _LocatePyAutoDir():
  sys.path.append(os.path.join(os.path.dirname(__file__),
                               os.pardir, 'pyautolib'))


_LocatePyAutoDir()
import pyauto_paths


def RunWithCorrectPythonIfNecessary():
  """Runs this script with the correct version of python if necessary.

  Different platforms and versions of pyautolib use different python versions.
  Instead of requiring testers and infrastructure to handle choosing the right
  version (and architecture), this will rerun the script with the correct
  version of python.

  Note, this function will either return after doing nothing, or will exit with
  the subprocess's return code.
  """
  def RunAgain():
    """Run the script again with the correct version of python.

    Note, this function does not return, but exits with the return code of the
    child.
    """
    if sys.platform == 'cygwin' or sys.platform.startswith('win'):
      cmd = [os.path.join(pyauto_paths.GetThirdPartyDir(), 'python_26',
                          'python_slave.exe')]
    elif sys.platform.startswith('darwin'):
      # Arch runs the specified architecture of a universal binary. Run
      # the 32 bit one.
      cmd = ['arch', '-i386', 'python2.6']
    elif sys.platform.startswith('linux'):
      cmd = ['python2.6']

    cmd.extend(sys.argv)
    print 'Running:', ' '.join(cmd)
    proc = subprocess.Popen(cmd)
    proc.wait()
    sys.exit(proc.returncode)

  def IsChromeOS():
    lsb_release = '/etc/lsb-release'
    if sys.platform.startswith('linux') and os.path.isfile(lsb_release):
      with open(lsb_release) as fp:
        contents = fp.read()
        return 'CHROMEOS_RELEASE_NAME=' in contents
    return False

  # Ensure this is the right python version (2.6 for chrome, 2.7 for chromeOS).
  if IsChromeOS():
    if sys.version_info[0:2] != (2, 7):
      cmd = ['python2.7'] + sys.argv
      print 'Running: ', ' '.join(cmd)
      proc = subprocess.Popen(cmd)
      proc.wait()
  else:
    if sys.version_info[0:2] != (2, 6):
      RunAgain()

  # Check this is the right bitness on mac.
  # platform.architecture() will not help us on mac, since multiple binaries
  # are stuffed inside the universal python binary.
  if sys.platform.startswith('darwin') and sys.maxint > 2**32:
    # User is running 64-bit python, but we should use 32-bit.
    RunAgain()


# Do not attempt to figure out python versions if
# DO_NOT_RESTART_PYTHON_FOR_PYAUTO is set.
if os.getenv('DO_NOT_RESTART_PYTHON_FOR_PYAUTO') is None:
  RunWithCorrectPythonIfNecessary()
else:
  print 'Will not try to restart with the correct version of python '\
        'as DO_NOT_RESTART_PYTHON_FOR_PYAUTO is set.'


try:
  import pyauto
except ImportError:
  print >>sys.stderr, 'Cannot import pyauto from %s' % sys.path
  raise


class Main(pyauto.Main):
  """Main program for running PyAuto functional tests."""

  def __init__(self):
    # Make scripts in this dir importable
    sys.path.append(os.path.dirname(__file__))
    pyauto.Main.__init__(self)

  def TestsDir(self):
    return os.path.dirname(__file__)


if __name__ == '__main__':
  Main()
