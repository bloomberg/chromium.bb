# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import _winreg

import settings


def VerifyRegistryEntries(entries):
  """Verifies that the current registry matches the specified criteria."""
  for key, entry in entries.iteritems():
    # TODO(sukolsak): Use unittest framework instead of prints.
    if VerifyRegistryEntry(key, entry):
      print 'Passed'
    else:
      print 'Failed'


def RootKeyConstant(key):
  if key == 'HKEY_CLASSES_ROOT':
    return _winreg.HKEY_CLASSES_ROOT
  if key == 'HKEY_CURRENT_USER':
    return _winreg.HKEY_CURRENT_USER
  if key == 'HKEY_LOCAL_MACHINE':
    return _winreg.HKEY_LOCAL_MACHINE
  if key == 'HKEY_USERS':
    return _winreg.HKEY_USERS
  # TODO(sukolsak): Use unittest framework instead of exceptions.
  raise Exception('Unknown registry key')


def VerifyRegistryEntry(key, entry):
  """Verifies that a registry entry exists or doesn't exist and has
  the specified value.

  Args:
    key: Name of the registry key.
    entry: A dictionary with the following keys and values:
      'expected' a boolean indicating whether the registry entry exists.
      'value' (optional) a string representing the value of the registry entry.

  Returns:
    A boolean indicating whether the registry entry matches the criteria.
  """
  expected = entry['expected']
  # TODO(sukolsak): Debug prints to be removed later.
  print settings.PRINT_VERIFIER_PREFIX + key,
  if expected:
    print 'exists...',
  else:
    print "doesn't exist...",
  root_key, sub_key = key.split('\\', 1)
  try:
    reg_key = _winreg.OpenKey(RootKeyConstant(root_key),
                              sub_key, 0, _winreg.KEY_READ)
  except WindowsError:
    return not expected
  if not expected:
    return False
  if 'value' in entry:
    # TODO(sukolsak): implement value
    pass
  return True
