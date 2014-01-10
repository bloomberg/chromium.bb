# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import errno
import glob
import os
import re
import subprocess
import sys

"""
This script searches for Visual Studio versions on the current system.

Pass in the preferred VS version on the command line, or pass "auto" for
autodetect.

This script prints a string containing the VS root directory. On failure it
returns the empty string.
"""

def _ConvertToCygpath(path):
  """Convert to cygwin path if we are using cygwin."""
  if sys.platform == 'cygwin':
    p = subprocess.Popen(['cygpath', path], stdout=subprocess.PIPE)
    path = p.communicate()[0].strip()
  return path


def _RegistryQueryBase(sysdir, key, value):
  """Use reg.exe to read a particular key.

  While ideally we might use the win32 module, we would like gyp to be
  python neutral, so for instance cygwin python lacks this module.

  Arguments:
    sysdir: The system subdirectory to attempt to launch reg.exe from.
    key: The registry key to read from.
    value: The particular value to read.
  Return:
    stdout from reg.exe, or None for failure.
  """
  # Setup params to pass to and attempt to launch reg.exe
  cmd = [os.path.join(os.environ.get('WINDIR', ''), sysdir, 'reg.exe'),
         'query', key]
  if value:
    cmd.extend(['/v', value])
  p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  # Obtain the stdout from reg.exe, reading to the end so p.returncode is valid
  # Note that the error text may be in [1] in some cases
  text = p.communicate()[0]
  # Check return code from reg.exe; officially 0==success and 1==error
  if p.returncode:
    return None
  return text


def _RegistryQuery(key, value=None):
  """Use reg.exe to read a particular key through _RegistryQueryBase.

  First tries to launch from %WinDir%\Sysnative to avoid WoW64 redirection. If
  that fails, it falls back to System32.  Sysnative is available on Vista and
  up and available on Windows Server 2003 and XP through KB patch 942589. Note
  that Sysnative will always fail if using 64-bit python due to it being a
  virtual directory and System32 will work correctly in the first place.

  KB 942589 - http://support.microsoft.com/kb/942589/en-us.

  Arguments:
    key: The registry key.
    value: The particular registry value to read (optional).
  Return:
    stdout from reg.exe, or None for failure.
  """
  text = None
  try:
    text = _RegistryQueryBase('Sysnative', key, value)
  except OSError, e:
    if e.errno == errno.ENOENT:
      text = _RegistryQueryBase('System32', key, value)
    else:
      raise
  return text


def _RegistryGetValue(key, value):
  """Use reg.exe to obtain the value of a registry key.

  Args:
    key: The registry key.
    value: The particular registry value to read.
  Return:
    contents of the registry key's value, or None on failure.
  """
  text = _RegistryQuery(key, value)
  if not text:
    return None
  # Extract value.
  match = re.search(r'REG_\w+\s+([^\r]+)\r\n', text)
  if not match:
    return None
  return match.group(1)


def _DetectVisualStudioVersion(versions_to_check, force_express):
  """Gets the path of the preferred Visual Studio version.

  Returns:
    The base path of Visual Studio based on the registry and a quick check if
    devenv.exe exists.

    Possibilities are:
      2010(e) - Visual Studio 2010 (10)
      2012(e) - Visual Studio 2012 (11)
      2013(e) - Visual Studio 2013 (12)
    Where (e) is e for express editions of MSVS and blank otherwise.
  """
  versions = []
  for version in versions_to_check:
    # Old method of searching for which VS version is installed
    # We don't use the 2010-encouraged-way because we also want to get the
    # path to the binaries, which it doesn't offer.
    keys = [r'HKLM\Software\Microsoft\VisualStudio\%s' % version,
            r'HKLM\Software\Wow6432Node\Microsoft\VisualStudio\%s' % version,
            r'HKLM\Software\Microsoft\VCExpress\%s' % version,
            r'HKLM\Software\Wow6432Node\Microsoft\VCExpress\%s' % version]
    for index in range(len(keys)):
      path = _RegistryGetValue(keys[index], 'InstallDir')
      if not path:
        continue
      path = _ConvertToCygpath(path)
      # Check for full.
      full_path = os.path.join(path, 'devenv.exe')
      express_path = os.path.join(path, '*express.exe')
      if not force_express and os.path.exists(full_path):
        return os.path.normpath(os.path.join(path, '..', '..'))
      # Check for express.
      elif glob.glob(express_path):
        return os.path.normpath(os.path.join(path, '..', '..'))

    # The old method above does not work when only SDK is installed.
    keys = [r'HKLM\Software\Microsoft\VisualStudio\SxS\VC7',
            r'HKLM\Software\Wow6432Node\Microsoft\VisualStudio\SxS\VC7']
    for index in range(len(keys)):
      path = _RegistryGetValue(keys[index], version)
      if not path:
        continue
      path = _ConvertToCygpath(path)
      return os.path.normpath(os.path.join(path, '..'))

  return None

if len(sys.argv) != 2:
  print 'Usage: get_visual_studio_path.py <version>'
  print 'Use "auto" for the version to autodetect.'
  sys.exit(2)
version_map = {
  'auto': ('10.0', '12.0', '11.0'),
  '2010': ('10.0',),
  '2010e': ('10.0',),
  '2012': ('11.0',),
  '2012e': ('11.0',),
  '2013': ('12.0',),
  '2013e': ('12.0',),
}

requested_version = sys.argv[1]
vs_path = _DetectVisualStudioVersion(version_map[requested_version],
                                     'e' in requested_version)
if not vs_path:
  # No Visual Studio version detected.
  print '""'  # Return empty string to .gn file.
  sys.exit(1);

# Return Visual Studio path to the .gn file.
print '"%s"' % vs_path
