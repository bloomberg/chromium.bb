# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromoting helper to install/uninstall host and replace pref pane."""

import abc
import os
import shutil
import sys
import subprocess


class ChromotingHelper(object):
  """Chromoting helper base class."""
  __metaclass__ = abc.ABCMeta

  @abc.abstractmethod
  def InstallHost(self, bin_dir):
    """Installs the chromoting host"""
    return

  @abc.abstractmethod
  def UninstallHost(self, bin_dir):
    """Uninstalls the chromoting host"""
    return


class ChromotingHelperMac(ChromotingHelper):
  """Chromoting Helper class for Mac.

  Installs/uninstalls host and replace the pref pane for testing purpose.
  """

  def InstallHost(self, bin_dir):
    """Installs host on Mac."""
    assert os.geteuid() == 0, 'Need superuser privileges'

    # Run most of the steps here with login user
    login_uid = os.getuid()
    os.seteuid(login_uid)

    # Change the working dir to the dir that has the host zip file
    current_dir = os.getcwd()
    pyautolib_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(bin_dir)
    host_dir = 'remoting-me2me-host-mac'
    output_dir = os.path.join(host_dir, 'output')

    # Remove remoting-me2me-host-mac dir just in case
    shutil.rmtree(host_dir, True)

    # Unzip the host archive and prepare the files/dirs
    subprocess.call('unzip remoting-me2me-host-mac.zip', shell=True)
    subprocess.call('mkdir ' + output_dir, shell=True)

    # Prepare security identity for code signing purpose
    os.seteuid(0)
    key_chain = '/Library/Keychains/ChromotingTest'
    password = '1111'
    key = os.path.join(pyautolib_dir, 'chromoting_key.p12')
    cert = os.path.join(pyautolib_dir, 'chromoting_cert.p12')
    subprocess.call(['security', 'delete-keychain', key_chain])
    subprocess.call(['security', 'create-keychain', '-p',
                     password, key_chain])
    subprocess.call(['security', 'import', key,
                     '-k', key_chain, '-P', password, '-A'])
    subprocess.call(['security', 'import', cert,
                     '-k', key_chain, '-P', password])
    os.seteuid(login_uid)

    # Sign the host
    do_signing = os.path.join(host_dir, 'do_signing.sh')
    subprocess.call(do_signing + ' ' + output_dir + ' ' + host_dir + ' ' +
                    key_chain + ' "Chromoting Test"', shell=True)

    # Remove security identify
    os.seteuid(0)
    subprocess.call(['security', 'delete-keychain', key_chain])
    os.seteuid(login_uid)

    # Figure out the dmg name
    version = ""
    for output_file in os.listdir(output_dir):
      if output_file.endswith('.dmg'):
        version = os.path.basename(output_file)[len('ChromotingHost-'):-4]

    # Mount before installation
    dmg = os.path.join(output_dir, 'ChromotingHost-' + version + '.dmg')
    subprocess.call('hdiutil' + ' mount ' + dmg, shell=True)

    # Install host
    os.seteuid(0)
    mpkg = os.path.join('/Volumes', 'Chromoting Host ' + version,
                        'Chromoting Host.pkg')
    subprocess.call(['/usr/sbin/installer', '-pkg',
                     mpkg, '-target', '/'])
    os.seteuid(login_uid)

    # Unmount after installation
    mounted = os.path.join('/Volumes', 'Chromoting Host ' + version)
    subprocess.call('hdiutil detach "' + mounted + '"', shell=True)

    # Clean up remoting-me2me-host-mac dir
    shutil.rmtree(host_dir, True)

    # Resume the original working dir
    os.chdir(current_dir)

  def UninstallHost(self, bin_dir):
    """Uninstalls host on Mac."""
    assert os.geteuid() == 0, 'Need superuser privileges'
    uninstall_app = os.path.join('/', 'Applications',
                                 'Chromoting Host Uninstaller.app',
                                 'Contents', 'MacOS',
                                 'remoting_host_uninstaller')
    subprocess.call([uninstall_app, '--no-ui'])

  def ReplacePrefPaneMac(self, operation):
    """Constructs mock pref pane to replace the actual pref pane on Mac."""
    assert os.geteuid() == 0, 'Need superuser privileges'

    pref_pane_dir = os.path.join('/Library', 'PreferencePanes')

    mock_pref_pane = os.path.join(pref_pane_dir, 'mock_pref_pane')
    pref_pane = os.path.join(pref_pane_dir,
                             'org.chromium.chromoting.prefPane')
    mock_pref_pane_python = os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        'mock_pref_pane.py')

    # When the symlink from real pref pane to mock pref pane exists,
    # mock pref pane will be modified to be a dir when the host is installed.
    # After the host is installed and mock pref pane is modified to be a file,
    # it will be a file until next host installation.
    if os.path.isdir(mock_pref_pane):
      shutil.rmtree(mock_pref_pane, True)
    elif os.path.isfile(mock_pref_pane):
      os.remove(mock_pref_pane)

    mock_pref_pane_file = open(mock_pref_pane, 'w')
    mock_pref_pane_file.write('#!/bin/bash\n')
    mock_pref_pane_file.write('\n')
    mock_pref_pane_file.write('suid-python'  +
                              ' ' + mock_pref_pane_python + ' ' + operation)
    mock_pref_pane_file.close()

    subprocess.call(['chmod', 'a+x', mock_pref_pane])

    # The real pref pane is a dir if the host is installed on a clean machine.
    # Once the test is run on the machine, real pref pane will be replaced to
    # a symlink.
    if os.path.isdir(pref_pane):
      shutil.rmtree(pref_pane, True)
    elif os.path.isfile(pref_pane):
      os.remove(pref_pane)

    subprocess.call(['ln', '-s', mock_pref_pane, pref_pane])


class ChromotingHelperWindows(ChromotingHelper):
  """Chromoting Helper class for Windows for installing/uninstalling host."""

  def InstallHost(self, bin_dir):
    """Installs host on Windows."""
    host_msi = os.path.join(bin_dir, 'chromoting.msi')
    subprocess.Popen(['msiexec', '/i', host_msi, '/passive']).wait()

  def UninstallHost(self, bin_dir):
    """Uninstalls host on Windows."""
    host_msi = os.path.join(bin_dir, 'chromoting.msi')
    subprocess.Popen(['msiexec', '/x', host_msi, '/passive']).wait()


def Main():
  """Main function to dispatch operations."""
  assert sys.platform.startswith('win') or \
      sys.platform.startswith('darwin'), \
      'Only support Windows and Mac'

  if sys.platform.startswith('win'):
    helper = ChromotingHelperWindows()
  elif sys.platform.startswith('darwin'):
    helper = ChromotingHelperMac()

  if sys.argv[1] == 'install':
    helper.InstallHost(sys.argv[2])
  elif sys.argv[1] == 'uninstall':
    helper.UninstallHost(sys.argv[2])
  elif sys.argv[1] in ['enable', 'disable', 'changepin']:
    assert sys.platform.startswith('darwin'), \
      'Replacing pref pane is Mac specific'
    helper.ReplacePrefPaneMac(sys.argv[1])
  else:
    print >>sys.stderr, 'Invalid syntax'
    return 1


if __name__ == '__main__':
  Main()