# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import _winreg


def VerifyRegistryEntries(entries, path_resolver):
  """Verifies that the current registry matches the specified criteria.

  Args:
    entries: A dictionary whose keys are registry keys and values are
        expectation dictionaries.
    path_resolver: A PathResolver object.
  """
  for key, expectation in entries.iteritems():
    VerifyRegistryEntry(key, expectation, path_resolver)


def RootKeyConstant(root_key):
  """Converts a root registry key string into a _winreg.HKEY_* constant."""
  root_key_mapping = {
      'HKEY_CLASSES_ROOT': _winreg.HKEY_CLASSES_ROOT,
      'HKEY_CURRENT_USER': _winreg.HKEY_CURRENT_USER,
      'HKEY_LOCAL_MACHINE': _winreg.HKEY_LOCAL_MACHINE,
      'HKEY_USERS': _winreg.HKEY_USERS,
  }
  if root_key not in root_key_mapping:
    raise KeyError("Unknown root registry key '%s'" % root_key)
  return root_key_mapping[root_key]


def ValueTypeConstant(value_type):
  """Converts a registry value type string into a _winreg.REG_* constant."""
  value_type_mapping = {
      'BINARY': _winreg.REG_BINARY,
      'DWORD': _winreg.REG_DWORD,
      'DWORD_LITTLE_ENDIAN': _winreg.REG_DWORD_LITTLE_ENDIAN,
      'DWORD_BIG_ENDIAN': _winreg.REG_DWORD_BIG_ENDIAN,
      'EXPAND_SZ': _winreg.REG_EXPAND_SZ,
      'LINK': _winreg.REG_LINK,
      'MULTI_SZ': _winreg.REG_MULTI_SZ,
      'NONE': _winreg.REG_NONE,
      'SZ': _winreg.REG_SZ,
  }
  if value_type not in value_type_mapping:
    raise KeyError("Unknown registry value type '%s'" % value_type)
  return value_type_mapping[value_type]


def VerifyRegistryEntry(key, expectation, path_resolver):
  """Verifies a registry key according to the |expectation|.

  The |expectation| specifies whether or not the registry key should exist
  (under 'exists') and optionally specifies expected 'values' for the key.

  Args:
    key: Name of the registry key. It is expanded using ResolvePath.
    expectation: A dictionary with the following keys and values:
        'exists' a boolean indicating whether the registry key should exist.
        'values' (optional) a dictionary where each key is a registry value and
            its associated value is a dictionary with the following key and
            values:
                'type' a string indicating the type of the registry value.
                'data' the associated data of the registry value. If it is a
                    string, it is expanded using ResolvePath.
    path_resolver: A PathResolver object.
  """
  resolved_key = path_resolver.ResolvePath(key)
  root_key, sub_key = resolved_key.split('\\', 1)
  try:
    # Query the Windows registry for the registry key. It will throw a
    # WindowsError if the key doesn't exist.
    key_handle = _winreg.OpenKey(RootKeyConstant(root_key), sub_key, 0,
                                 _winreg.KEY_QUERY_VALUE)
  except WindowsError:
    # Key doesn't exist. See that it matches the expectation.
    assert not expectation['exists'], ('Registry key %s is missing' %
                                       resolved_key)
    return
  # The key exists, see that it matches the expectation.
  assert expectation['exists'], ('Registry key %s exists' % resolved_key)

  # Verify the expected values.
  if 'values' not in expectation:
    return
  for value, value_expectation in expectation['values'].iteritems():
    # Query the value. It will throw a WindowsError if the value doesn't exist.
    try:
      data, value_type = _winreg.QueryValueEx(key_handle, value)
    except WindowsError:
      raise KeyError("Value '%s' of registry key %s is missing" % (
          value, resolved_key))

    # Verify the type of the value.
    expected_value_type = value_expectation['type']
    assert ValueTypeConstant(expected_value_type) == value_type, \
        "Value '%s' of registry key %s has unexpected type '%s'" % (
            value, resolved_key, expected_value_type)

    # Verify the associated data of the value.
    expected_data = value_expectation['data']
    if isinstance(expected_data, basestring):
      expected_data = path_resolver.ResolvePath(expected_data)
    assert expected_data == data, \
        ("Value '%s' of registry key %s has unexpected data.\n"
         "  Expected: %s\n"
         "  Actual: %s" % (value, resolved_key, expected_data, data))
