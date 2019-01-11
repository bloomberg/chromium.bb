# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for kernel commandline strings."""

from __future__ import print_function

import collections
import re


class KeyValue(object):
  """Stores a key(=value)."""

  def __init__(self, key, separator, value):
    self.key = key
    self.separator = separator
    self.value = value

  def __ne__(self, other):
    return not self.__eq__(other)

  def __eq__(self, other):
    return (isinstance(other, KeyValue) and
            self.key == other.key and
            self.separator == other.separator and
            self.value == other.value)

  def __str__(self):
    return self.Format()

  def Format(self):
    """Return the key(=value) as a string."""
    if isinstance(self.value, basestring):
      value = self.value
    else:
      value = '"%s"' % ' '.join(x.Format() for x in self.value)
    return '%s%s%s' % (self.key, self.separator, value)


_KEYVALUE_RE = r'(?:(--|[^\s=]+)(?:(=)("[^"]*"|[^\s"]*))?)'
_VALID_CMDLINE_RE = (r'\s*'
                     r'%s?'
                     r'(\s+%s)*'
                     r'\s*$') % (_KEYVALUE_RE, _KEYVALUE_RE)
def _ParseKeyValue(cmd_string):
  """Tokenize a string of key(=value) pairs.

  key(=value):
    key: One or more characters excluding whitespace and '='.
    value: If present, is zero or more non-whitespace characters, or an
        arbitrary string surrounded by double-quote (").

  Args:
    cmd_string: Whitespace separated collection of 'key', 'key=', and/or
        'key=value'.

  Returns:
    List of KeyValue objects.

  Raises:
    ValueError: One or more tokens is invalid.
  """
  # We use lookbehind to validate tokens, so we need a leading space on
  # |cmd_string| to match the leading argument.
  valid = re.match(_VALID_CMDLINE_RE, cmd_string)
  if not valid:
    raise ValueError(cmd_string)
  args = re.findall(_KEYVALUE_RE, cmd_string)
  if args:
    return [KeyValue(*x) for x in args]
  return []


class CommandLine(object):
  """Make parsing the kernel command line easier.

  Attributes:
    kern_args: Kernel arguments (before any '--')
    init_args: Any arguments for init (after the first '--')
  """

  def __init__(self, cmdline):
    args = _ParseKeyValue(cmdline)
    self.kern_args = args
    self.init_args = []
    for idx, arg  in enumerate(args):
      if arg.key == '--' and not arg.separator:
        self.kern_args = args[:idx]
        self.init_args = args[idx + 1:]
        # We are done looking at args.
        break

  def __str__(self):
    return self.Format()

  def Format(self):
    """Format the commandline for use."""
    parts = [x.Format() for x in self.kern_args]
    if self.init_args:
      parts.append('--')
      parts += [x.Format() for x in self.init_args]
    return ' '.join(parts)

  def GetKernelParameter(self, key=None, index=None, default=None):
    """Get kernel argument.

    Get an argument from kern_args.  If |key| and |index| are specified, then
    |index| is used.

    Args:
      key: return the first value for |key|.
      index: return the argument at index |index| (first argument is 0).
      default: Value to return if nothing is found.

    Returns:
      If found, returns the KeyValue from the cmdline.
      Otherwise returns the provided default.

    Raises:
      IndexError on non-None invalid |index|.
      IndexError if neither |key| nor |index| is specifiexd.
    """
    if index is not None:
      return self.kern_args[index]
    if key is None:
      raise IndexError('Neither key nor index specified')
    for arg in self.kern_args:
      if arg.key == key:
        return arg
    return default

  def SetKernelParameter(self, arg, index=None):
    """Set a kernel argument.

    If |arg.value| is specified and |arg.separator| is not, '=' is used for the
    separator.

    Args:
      arg: KeyValue to place in kernel cmdline.  If |arg.value| is unquoted and
          has whitespace, double-quotes will be added to preserve spaces.
      index: |index| to overwrite.  If not specified, the first argument
          matching |arg.key| will be used.

    Returns:
      Index of argument that was set.
    """
    # Override arg.separator and/or arg.value as needed.
    sep = '=' if not arg.separator and arg.value else arg.separator
    val = arg.value
    if not val.startswith('"') and re.search(r'\s', val):
      val = '"%s"' % val
    if sep != arg.separator or val != arg.value:
      arg = KeyValue(arg.key, sep, val)

    if index is None:
      # index not given, set index to the first occurrence of arg.key.
      for index in range(len(self.kern_args)):
        if self.kern_args[index].key == arg.key:
          break
      else:
        # We did not find the key at all.  Append this to the cmdline.
        index = len(self.kern_args)
    if index == len(self.kern_args):
      self.kern_args.append(arg)
    else:
      self.kern_args[index] = arg
    return index

  def GetDmConfig(self):
    """Return first dm= argument for processing."""
    value = self.GetKernelParameter('dm').value[1:-1]
    if value:
      return DmConfig(value)
    return None

  def SetDmConfig(self, dm_config):
    """Set the dm= argument to a DmConfig.

    Args:
      dm_config: DmConfig instance to use.

    Returns:
      Index of argument that was set.
    """
    return self.SetKernelParameter(KeyValue('dm', '=', dm_config.Format()))


class DmConfig(object):
  """Parse the dm= parameter.

  Args:
    boot_arg: contents of the quoted dm="..." kernel cmdline element, without
        the quotes.

  Attributes:
    num_devices: Number of devices defined in this dmsetup config.
    devices: OrderedDict of devices, by device name.
  """

  def __init__(self, boot_arg):
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
        [', '.join(x.Format() for x in self.devices.values())])


class DmDevice(object):
  """A single device in the dm= kernel parameter.

  Args:
    config_lines: List of lines to process.  Excess elements are ignored.

  Attributes:
    name: Name of the device
    uuid: Uuid of the device
    flags: One of 'ro' or 'rw'
    num_rows: Number of dmsetup config lines (|config_lines|) used by this
        device.
    rows: List of DmLine objects for the device.
  """

  def __init__(self, config_lines):
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
    """Return the device formatted for the kernel."""
    return ','.join(
        ['%s %s %s %d' % (self.name, self.uuid, self.flags, self.num_rows)] +
        [x.Format() for x in self.rows])


class DmLine(object):
  """A single line from the dmsetup config for a device.

  Attributes:
    start: Logical start sector
    num: Number of sectors.
    target_type: target_type.  See dmsetup(8).
    args: list of KeyValue args for the line.
  """

  def __init__(self, line):
    """Parse a single line of dmsetup config."""
    # Allow leading whitespace.
    start, num, target, args = line.strip().split(' ', 3)
    self.start = int(start)
    self.num = int(num)
    self.target_type = target
    self.args = _ParseKeyValue(args)

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
        self.start, self.num, self.target_type, ' '.join(
            x.Format() for x in self.args))])
