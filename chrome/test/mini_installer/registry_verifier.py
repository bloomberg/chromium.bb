# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import _winreg


def VerifyRegistryEntries(entries):
  """Verifies that the current registry matches the specified criteria.

  Args:
    entries: A dictionary whose keys are registry keys and values are
        expectation dictionaries.
  """
  for key, expectation in entries.iteritems():
    VerifyRegistryEntry(key, expectation)


def RootKeyConstant(key):
  """Converts a root registry key string into a _winreg.HKEY_* constant."""
  if key == 'HKEY_CLASSES_ROOT':
    return _winreg.HKEY_CLASSES_ROOT
  if key == 'HKEY_CURRENT_USER':
    return _winreg.HKEY_CURRENT_USER
  if key == 'HKEY_LOCAL_MACHINE':
    return _winreg.HKEY_LOCAL_MACHINE
  if key == 'HKEY_USERS':
    return _winreg.HKEY_USERS
  raise KeyError("Unknown root registry key '%s'" % key)


def VerifyRegistryEntry(key, expectation):
  """Verifies a registry key according to the |expectation|.

  The |expectation| specifies whether or not the registry key should exist
  (under 'exists') and optionally specifies an expected 'value' for the key.

  Args:
    key: Name of the registry key.
    expectation: A dictionary with the following keys and values:
        'exists' a boolean indicating whether the registry entry should exist.
        'value' (optional) a string representing the expected value for
            the key.
  """
  root_key, sub_key = key.split('\\', 1)
  try:
    # Query the Windows registry for the registry key. It will throw a
    # WindowsError if the key doesn't exist.
    _ = _winreg.OpenKey(RootKeyConstant(root_key), sub_key, 0, _winreg.KEY_READ)
  except WindowsError:
    # Key doesn't exist. See that it matches the expectation.
    assert not expectation['exists'], 'Registry entry %s is missing' % key
    return
  # The key exists, see that it matches the expectation.
  assert expectation['exists'], 'Registry entry %s exists' % key
  # TODO(sukolsak): Verify the expected value.
