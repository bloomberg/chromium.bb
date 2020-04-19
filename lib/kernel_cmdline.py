# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for kernel commandline strings."""

from __future__ import print_function

import collections
import re
import sys

import six


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


class KernelArg(object):
  """Stores a arg(=value).

  Valid KernelArgs are: 'arg', 'arg=', and 'arg=value'.
  """

  def __init__(self, arg, value):
    """Initialize the instance.

    Args:
      arg: Value to use for arg.
      value: Value to use.  If |value| is not None, then the output argument
          will take the form of 'arg=value'.  (An empty string yields 'arg='.)

    Raises:
      ValueError: Invalid quotes in |value|.
    """
    if value and (not isinstance(value, six.string_types) or
                  '"' in value[1:-1] or
                  value.startswith('"') != value.endswith('"')):
      raise ValueError(value)
    self.arg = arg
    self.value = value

  def __ne__(self, other):
    return not self.__eq__(other)

  def __eq__(self, other):
    # Surrounding quotes in .value are optional.
    return (isinstance(other, KernelArg) and
            self.arg == other.arg and
            (self.value == other.value or self.value == '"%s"' % other.value
             or '"%s"' % self.value == other.value))

  def __str__(self):
    return self.Format()

  def __hash__(self):
    return hash(str(self))

  def Format(self):
    """Return the arg(=value) as a string.

    Values with whitespace will have double-quotes added if not present.
    A value of None yields just the arg, and no equal-sign.
    """
    if self.value is None:
      return str(self.arg)
    else:
      value = str(self.value)
      if not value.startswith('"') and re.search(r'\s', value):
        value = '"%s"' % value
      return '%s=%s' % (self.arg, value)


# Token: [one or more characters excluding whitespace and equals] possibly
# followed by an equal sign (=) and optional value string, consisting of either
# non-whitespace, or double-quotes surrounding a string which may include
# whitespace, but no double-quotes (").
_KEYVALUE_RE = r'(?:(--|[^\s=]+)(?:(=)("[^"]*"|[^\s"]*))?)'
# Multiple concatenating strings used for readability:
# <Whitespace>  <token>? <whitespace followed by token>* <whitespace>
_VALID_CMDLINE_RE = (
    r'\s*'  r'%s?'  r'(\s+%s)*'  r'\s*$') % (_KEYVALUE_RE, _KEYVALUE_RE)

class KernelArgList(collections.MutableMapping, collections.MutableSequence):
  """A tokenized collection of key(=value) pairs.

  Behaves as a list, with some extra features.

  Differences from list:
    Creation: if given a string, the string is split into KernelArg elements.
    Indexing: if given a string for |index|, the first element with
        |element.key| == |index| is used.
  """

  def __init__(self, data=None):
    """Initialize the KernelArgList.

    Args:
      data: Either an iterable yielding KernelArg elements, or string containing
          whitespace-separated tokens of 'arg', 'arg=', and/or 'arg=value'.
          |arg| is any string not containing whitespace or '='.
          |value| is any string.  Use double-quotes (") if there is whitespace.
    """
    # If we got a string, split it into KernelArg pairs.  If not, just pass
    # it through list().
    if isinstance(data, six.string_types):
      valid = re.match(_VALID_CMDLINE_RE, data)
      if not valid:
        raise ValueError(data)
      args = re.findall(_KEYVALUE_RE, data)
      if args:
        self._data = [KernelArg(k, v if s else None) for k, s, v in args]
      else:
        self._data = []
    elif data is None:
      self._data = []
    else:
      self._data = list(data)
      for kv in self._data:
        if not isinstance(kv, KernelArg):
          raise ValueError(kv)

  def __len__(self):
    return len(self._data)

  def __iter__(self):
    return iter(self._data)

  def __eq__(self, other):
    if isinstance(other, KernelArgList):
      # pylint: disable=protected-access
      return self._data == other._data
    else:
      # Comparing to a list of KeyValues is permitted.
      return self._data == other

  def __ne__(self, other):
    return not self.__eq__(other)

  def __add__(self, other):
    # pylint: disable=protected-access
    return KernelArgList(self._data + other._data)

  def __iadd__(self, other):
    # pylint: disable=protected-access
    self._data += other._data
    return self

  def __contains__(self, item):
    """Return True if |item| is in the list.

    Args:
      item: Either a KernelArg (which is searched for precisely), or a string
           argument (which is compared against only |entry.arg|, ignoring
           |entry.value|).
    """
    if isinstance(item, six.string_types):
      for kern_arg in self._data:
        if kern_arg.arg == item:
          return True
      return False
    else:
      return item in self._data

  def __delitem__(self, key):
    """Delete |key| from the list.

    If |key| is a string, it refers to the first occurance of |key| as an
    argument in the list.

    Args:
      key: Either a slice, an integer index, or a string argument to delete.
          A string is converted to a numeric index via index().
    """
    if isinstance(key, six.string_types):
      idx = self.index(key)
      if idx is None:
        raise KeyError(key)
      del self._data[idx]
    else:
      del self._data[key]

  def __getitem__(self, key):
    """Get |key| from the list.

    If |key| is a string, it refers to the first occurance of |key| as an
    argument in the list.

    Args:
      key: Either a slice, an integer index, or a string argument to get.
          A string is converted to a numeric index via index().

    Raises:
      KeyError: |key| is not found.
    """
    if isinstance(key, slice):
      return KernelArgList(self._data[key])
    idx = self.index(key)
    if idx is None:
      raise KeyError(key)
    return self._data[idx]

  def __setitem__(self, key, value):
    """Set |key| to |value|.

    If |key| is a string, it refers to the first occurance of |key| as an
    argument in the list.

    Args:
      key: A slice, an integer index, or a string |arg| name.
      value: If |key| is a slice, the slice will be set to KernelArgList(value).
          Otherwise |value| must be a KernelArg.
    """
    if isinstance(key, slice):
      self._data[key] = KernelArgList(value)
      return
    if not isinstance(value, KernelArg):
      raise ValueError(value)
    # Convert string keys into integer indexes.
    if isinstance(key, six.string_types):
      idx = self.index(key)
      # Setting a non-existent string index does an append.
      if idx is None:
        self._data.append(value)
      else:
        self._data[idx] = value
    else:
      self._data[key] = value

  def get(self, key, default=None):
    """Return the first element with arg=|key|.

    Args:
      key: An integer index, or a string |arg| name.
      default: Return value if |key| is not found.

    Returns:
      First KernelArg where arg == |key|.
    """
    idx = self.index(key)
    if idx is None:
      return default
    else:
      return self._data[idx]

  def index(self, key):
    """Find the index for |key|.

    Args:
      key: Key to find.

    Returns:
      The index of the first element where |element.key| == |key|.  If that is
      not found, None is returned.
    """
    if isinstance(key, int):
      return key
    for idx, ka in enumerate(self._data):
      if ka.arg == key:
        return idx
    return None

  def insert(self, index, obj):
    """Insert |obj| before |index|.

    Args:
      index: An integer index, or a string |arg| name.
      obj: KernelArg to insert.

    Raises:
      KeyError: String |index| given and not found.
      ValueError: |obj| is not a KernelArg.
    """
    if not isinstance(obj, KernelArg):
      raise ValueError(obj)
    # Convert string index to an integer index.
    if isinstance(index, six.string_types):
      key = index
      index = self.index(index)
      if index is None:
        raise KeyError(key)
    self._data.insert(index, obj)

  def update(self, other=None, **kwargs):  # pylint: disable=arguments-differ
    """Update the list.

    Set elements of the list.  Depending on the type of |other|, one of the
    following is done:
      KernelArgList([item]): self[item.arg] = item
      dict {key: value}: self[key] = KernelArg(key, value)
      iterable (arg, value): self[arg] = KernelArg(arg, value)
    Also supports keyword arguments, which are passed through KernelArg()
    similar to "dict" above.

    Args:
      other: Either a KernelArgList, a dict of {arg: value}, or an iterable of
          (arg, value) pairs (which will be passed to KernelArg())
      **kwargs: |key| and |value| are passed to KernelArg.
    """
    if other:
      if isinstance(other, KernelArgList):
        for kernel_arg in other:
          self[kernel_arg.arg] = kernel_arg
      elif isinstance(other, dict):
        for arg, value in other.items():
          self[arg] = KernelArg(arg, value)
      else:
        for arg, value in other:
          self[arg] = KernelArg(arg, value)

    for arg, value in kwargs.items():
      self[arg] = KernelArg(arg, value)

  def __str__(self):
    return self.Format()

  def Format(self, separator=' '):
    """Return the list of key(=value)s as a string.

    Args:
      separator: Delimiter between list elements.
    """
    return separator.join(str(x) for x in self._data)


class CommandLine(object):
  """Make parsing the kernel command line easier.

  Attributes:
    kern_args: Kernel arguments (before any '--').
    init_args: Any arguments for init (after the first '--').
  """

  def __init__(self, cmdline):
    args = KernelArgList(cmdline)
    idx = args.index('--')
    if idx is None:
      idx = len(args)
    self.kern_args = args[:idx]
    self.init_args = args[idx + 1:]

  def __ne__(self, other):
    return not self.__eq__(other)

  def __eq__(self, other):
    return (isinstance(other, CommandLine) and
            self.kern_args == other.kern_args and
            self.init_args == other.init_args)

  def __str__(self):
    return self.Format()

  def Format(self):
    """Format the commandline for use."""
    parts = [str(self.kern_args)]
    if self.init_args:
      parts.append(str(self.init_args))
    return ' -- '.join(parts)

  def GetKernelParameter(self, what, default=None):
    """Get kernel argument.

    Get an argument from kern_args.

    Args:
      what: Either an integer index, or a key to search for.
      default: Value to return if nothing is found.

    Returns:
      If found, returns the KernelArg from the kernel cmdline.
      Otherwise returns the provided default.

    Raises:
      IndexError on non-None invalid integer |what|.
      TypeError on non-None non-integer |what| that does not match |element.key|
          for any element.
    """
    return self.kern_args.get(what, default=default)

  def SetKernelParameter(self, key, value):
    """Set a kernel argument.

    Args:
      key: |key| for KernelArg.
      value: |value| for KernelArg.
    """
    self.kern_args.update({key:value})

  def GetDmConfig(self):
    """Return first dm= argument for processing."""
    dm_kv = self.GetKernelParameter('dm')
    if dm_kv:
      return DmConfig(dm_kv.value)
    return None

  def SetDmConfig(self, dm_config):
    """Set the dm= argument to a DmConfig.

    Args:
      dm_config: DmConfig instance to use.
    """
    if dm_config is None:
      if 'dm' in self.kern_args:
        del self.kern_args['dm']
      return
    self.SetKernelParameter('dm', str(dm_config))


class DmConfig(object):
  """Parse the dm= parameter.

  Attributes:
    num_devices: Number of devices defined in this dmsetup config.
    devices: OrderedDict of devices, by device name.
  """

  def __init__(self, boot_arg):
    """Initialize.

    Args:
      boot_arg: contents of the quoted dm="..." kernel cmdline element, with or
          without surrounding quotes.
    """
    if boot_arg.startswith('"') and boot_arg.endswith('"'):
      boot_arg = boot_arg[1:-1]
    num_devices, devices = boot_arg.split(' ', 1)
    self.num_devices = int(num_devices)
    lines = devices.split(',')
    self.devices = collections.OrderedDict()
    idx = 0
    for _ in range(self.num_devices):
      dev = DmDevice(lines[idx:])
      self.devices[dev.name] = dev
      idx += dev.num_rows + 1

  def __ne__(self, other):
    return not self.__eq__(other)

  def __eq__(self, other):
    return (isinstance(other, DmConfig) and
            self.num_devices == other.num_devices and
            self.devices == other.devices)

  def __str__(self):
    return self.Format()

  def Format(self):
    """Format dm= value."""
    return ''.join(
        ['%d ' % self.num_devices] +
        [', '.join(str(x) for x in self.devices.values())])


class DmDevice(object):
  """A single device in the dm= kernel parameter.

  Attributes:
    name: Name of the device.
    uuid: Uuid of the device.
    flags: One of 'ro' or 'rw'.
    num_rows: Number of dmsetup config lines (|config_lines|) used by this
        device.
    rows: List of DmLine objects for the device.
  """

  def __init__(self, config_lines):
    """Initialize.

    Args:
      config_lines: List of lines to process.  Excess elements are ignored.
    """
    name, uuid, flags, rows = config_lines[0].split()
    self.name = name
    self.uuid = uuid
    self.flags = flags
    self.num_rows = int(rows)
    self.rows = [DmLine(row) for row in config_lines[1:self.num_rows + 1]]

  def __ne__(self, other):
    return not self.__eq__(other)

  def __eq__(self, other):
    return (isinstance(other, DmDevice) and
            self.name == other.name and
            self.uuid == other.uuid and
            self.flags == other.flags and
            self.num_rows == other.num_rows and
            self.rows == other.rows)

  def __str__(self):
    return self.Format()

  def Format(self):
    """Return the device formatted for the kernel, without quotes."""
    return ','.join(
        ['%s %s %s %d' % (self.name, self.uuid, self.flags, self.num_rows)] +
        [str(x) for x in self.rows])

  def GetVerityArg(self, key, default=None):
    """Return the specified argument KernelArg for the first verity line.

    If there are multiple verity lines, only the first one is examined.

    Args:
      key: verity argument to find.
      default: Return this if |key| is not found.
    """
    for row in self.rows:
      if row.target_type == 'verity':
        return row.args.get(key, default=default)
    return default

  def UpdateVerityArg(self, key, value):
    """Update any |key| argument in any 'verity' config line to the new value.

    If no verity lines contain |key|, then add it to all of them.

    Args:
      key: Key to update if found.  Passed to KernelArg().
      value: Passed to KernelArg().
    """
    for idx in range(len(self.rows)):
      if (self.rows[idx].target_type == 'verity' and
          key in self.rows[idx].args):
        self.rows[idx].args[key] = KernelArg(key, value)
    for idx in range(len(self.rows)):
      if self.rows[idx].target_type == 'verity':
        self.rows[idx].args[key] = KernelArg(key, value)


class DmLine(object):
  """A single line from the dmsetup config for a device.

  Attributes:
    start: Logical start sector
    num: Number of sectors.
    target_type: target_type.  See dmsetup(8).
    args: list of KernelArg args for the line.
  """

  def __init__(self, line):
    """Parse a single line of dmsetup config."""
    # Allow leading whitespace.
    start, num, target, args = line.strip().split(' ', 3)
    self.start = int(start)
    self.num = int(num)
    self.target_type = target
    self.args = KernelArgList(args)

  def __ne__(self, other):
    return not self.__eq__(other)

  def __eq__(self, other):
    return (isinstance(other, DmLine) and
            self.start == other.start and
            self.num == other.num and
            self.target_type == other.target_type and
            self.args == other.args)

  def __str__(self):
    return self.Format()

  def Format(self):
    """Format this line of the dmsetup config."""
    return ','.join(['%d %d %s %s' % (
        self.start, self.num, self.target_type, str(self.args))])
