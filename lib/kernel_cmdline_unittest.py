# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the kernel_cmdline module."""

from __future__ import print_function

from chromite.lib import cros_test_lib
from chromite.lib import kernel_cmdline

# pylint: disable=protected-access

# A partial, but sufficiently complicated command line.  DmConfigTest uses
# more complicated configs, with multiple devices.  DM is split out here for
# CommandLineTest.test*DmConfig().
DM = (
    '1 vroot none ro 1,0 3891200 verity payload=PARTUUID=%U/PARTNROFF=1 '
    'hashtree=PARTUUID=%U/PARTNROFF=1 hashstart=3891200 alg=sha1 '
    'root_hexdigest=9999999999999999999999999999999999999999 '
    'salt=bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb')
CMDLINE = (
    'console= loglevel=7 init=/sbin/init cros_secure oops=panic panic=-1 '
    'root=/dev/dm-0 rootwait ro dm_verity.error_behavior=3 '
    'dm_verity.max_bios=-1 dm_verity.dev_wait=1 dm="' + DM +
    '" noinitrd cros_debug vt.global_cursor_default=0 kern_guid=%U '
    '-- run_level=3')


class KeyValueTest(cros_test_lib.TestCase):
  """Test KeyValue"""

  def testKeyOnly(self):
    """Expands key-only arg"""
    self.assertEqual('key', kernel_cmdline.KeyValue('key', '', '').Format())

  def testKeyEqual(self):
    """Expands key= arg"""
    self.assertEqual('key=', kernel_cmdline.KeyValue('key', '=', '').Format())

  def testKeyValue(self):
    """Expands key=value arg"""
    self.assertEqual('key=value',
                     kernel_cmdline.KeyValue('key', '=', 'value').Format())

  def testRecursion(self):
    """Expands key=list-of-KeyValue"""
    self.assertEqual('a="b c= d=e"',
                     kernel_cmdline.KeyValue('a', '=', [
                         kernel_cmdline.KeyValue('b', '', ''),
                         kernel_cmdline.KeyValue('c', '=', ''),
                         kernel_cmdline.KeyValue('d', '=', 'e')]).Format())


class ParseKeyValueTest(cros_test_lib.TestCase):
  """Test _ParseKeyValue()."""

  def testSimple(self):
    """Test a simple command line string."""
    expected = [
        kernel_cmdline.KeyValue('a', '', ''),
        kernel_cmdline.KeyValue('b', '=', ''),
        kernel_cmdline.KeyValue('c', '=', 'd'),
        kernel_cmdline.KeyValue('e.f', '=', '"d e"'),
        kernel_cmdline.KeyValue('a', '', ''),
        kernel_cmdline.KeyValue('--', '', ''),
        kernel_cmdline.KeyValue('x', '', ''),
        kernel_cmdline.KeyValue('y', '=', ''),
        kernel_cmdline.KeyValue('z', '=', 'zz'),
        kernel_cmdline.KeyValue('y', '', '')]
    args = kernel_cmdline._ParseKeyValue('a b= c=d e.f="d e" a -- x y= z=zz y')
    self.assertEqual(args, expected)

  def testReturnsEmptyList(self):
    """Test that strings with no arguments return an emtpy list."""
    self.assertEqual([], kernel_cmdline._ParseKeyValue('  '))

  def testRejectsInvalidCmdline(self):
    """Test that invalid command line strings are rejected."""
    # First and non-First are different in the re.
    with self.assertRaises(ValueError):
      kernel_cmdline._ParseKeyValue('=3')
    with self.assertRaises(ValueError):
      kernel_cmdline._ParseKeyValue('a =3')
    # Various bad quote usages.
    with self.assertRaises(ValueError):
      kernel_cmdline._ParseKeyValue('a b="foo"3 c')
    with self.assertRaises(ValueError):
      kernel_cmdline._ParseKeyValue('a b=" c')


class CommandLineTest(cros_test_lib.TestCase):
  """Test the CommandLine class"""

  def testSimple(self):
    """Test a simple command line string."""
    expected_kern = [
        kernel_cmdline.KeyValue('a', '', ''),
        kernel_cmdline.KeyValue('b', '=', ''),
        kernel_cmdline.KeyValue('c', '=', 'd'),
        kernel_cmdline.KeyValue('e.f', '=', 'd'),
        kernel_cmdline.KeyValue('a', '', '')]
    expected_init = [
        kernel_cmdline.KeyValue('x', '', ''),
        kernel_cmdline.KeyValue('y', '=', ''),
        kernel_cmdline.KeyValue('z', '=', 'zz'),
        kernel_cmdline.KeyValue('y', '', '')]
    cmd = kernel_cmdline.CommandLine('a b= c=d e.f=d a -- x y= z=zz y')
    self.assertEqual(cmd.kern_args, expected_kern)
    self.assertEqual(cmd.init_args, expected_init)

  def testEmptyInit(self):
    """Test that 'a --' behaves as expected."""
    expected_kern = [kernel_cmdline.KeyValue('a', '', '')]
    expected_init = []
    cmd = kernel_cmdline.CommandLine('a --')
    self.assertEqual(expected_kern, cmd.kern_args)
    self.assertEqual(expected_init, cmd.init_args)

  def testEmptyKern(self):
    """Test that '-- a' behaves as expected."""
    expected_kern = []
    expected_init = [kernel_cmdline.KeyValue('a', '', '')]
    cmd = kernel_cmdline.CommandLine('-- a')
    self.assertEqual(expected_kern, cmd.kern_args)
    self.assertEqual(expected_init, cmd.init_args)

  def testEmptyArg(self):
    """Test that '' behaves as expected."""
    expected_kern = []
    expected_init = []
    cmd = kernel_cmdline.CommandLine('')
    self.assertEqual(expected_kern, cmd.kern_args)
    self.assertEqual(expected_init, cmd.init_args)

  def testDashesOnly(self):
    """Test that '--' behaves as expected."""
    expected_kern = []
    expected_init = []
    cmd = kernel_cmdline.CommandLine('--')
    self.assertEqual(expected_kern, cmd.kern_args)
    self.assertEqual(expected_init, cmd.init_args)

  def testExpandsDm(self):
    """Test that we do not expand dm="..."."""
    expected_kern = [
        kernel_cmdline.KeyValue('a', '', ''),
        kernel_cmdline.KeyValue('dm', '=', '"1 vroot b=c 1,0 1200"'),
        kernel_cmdline.KeyValue('c.d', '=', 'e')]
    expected_init = [
        kernel_cmdline.KeyValue('dm', '=', '"not split"'),
        kernel_cmdline.KeyValue('a', '', '')]
    cmd = kernel_cmdline.CommandLine(
        'a dm="1 vroot b=c 1,0 1200" c.d=e -- dm="not split" a')
    self.assertEqual(expected_kern, cmd.kern_args)
    self.assertEqual(expected_init, cmd.init_args)

  def testGetKernelParameterIndex(self):
    """Test GetKernelParameter() with index="""
    cmd = kernel_cmdline.CommandLine('a b c b=3')
    self.assertEqual(
        kernel_cmdline.KeyValue('a', '', ''), cmd.GetKernelParameter(index=0))
    self.assertEqual(
        kernel_cmdline.KeyValue('b', '', ''), cmd.GetKernelParameter(index=1))
    self.assertEqual(
        kernel_cmdline.KeyValue('c', '', ''), cmd.GetKernelParameter(index=2))
    self.assertEqual(
        kernel_cmdline.KeyValue('b', '=', '3'), cmd.GetKernelParameter(index=3))
    with self.assertRaises(IndexError):
      cmd.GetKernelParameter(index=4)

  def testGetKernelParameter(self):
    """Test GetKernelParameter() with key"""
    cmd = kernel_cmdline.CommandLine('a b c=1 b=3')
    self.assertEqual(
        kernel_cmdline.KeyValue('a', '', ''), cmd.GetKernelParameter('a'))
    self.assertEqual(
        kernel_cmdline.KeyValue('b', '', ''), cmd.GetKernelParameter('b'))
    self.assertEqual(
        kernel_cmdline.KeyValue('c', '=', '1'), cmd.GetKernelParameter('c'))
    self.assertEqual('default', cmd.GetKernelParameter('d', default='default'))

  def testSetKernelParameterIndex(self):
    """Test SetKernelParameter() with index="""
    cmd = kernel_cmdline.CommandLine('a b c')
    self.assertEqual(0, cmd.SetKernelParameter(
        kernel_cmdline.KeyValue('d', '=', ''), index=0))
    self.assertEqual([
        kernel_cmdline.KeyValue('d', '=', ''),
        kernel_cmdline.KeyValue('b', '', ''),
        kernel_cmdline.KeyValue('c', '', '')], cmd.kern_args)

  def testSetKernelParameterIndexAppends(self):
    """Test SetKernelParameter() appends with index = len+1"""
    cmd = kernel_cmdline.CommandLine('a b c')
    self.assertEqual(3, cmd.SetKernelParameter(
        kernel_cmdline.KeyValue('d', '=', ''), index=3))
    self.assertEqual([
        kernel_cmdline.KeyValue('a', '', ''),
        kernel_cmdline.KeyValue('b', '', ''),
        kernel_cmdline.KeyValue('c', '', ''),
        kernel_cmdline.KeyValue('d', '=', '')], cmd.kern_args)

  def testSetKernelParameterIndexAssertsOnBadIndex(self):
    """Test SetKernelParameter() appends with index > len+1"""
    cmd = kernel_cmdline.CommandLine('a b c')
    with self.assertRaises(IndexError):
      cmd.SetKernelParameter(kernel_cmdline.KeyValue('d', '=', ''), index=5)

  def testSetKernelParameter(self):
    """Test SetKernelParameter() with key"""
    cmd = kernel_cmdline.CommandLine('a b c')
    self.assertEqual(
        1, cmd.SetKernelParameter(kernel_cmdline.KeyValue('b', '=', 'f')))
    self.assertEqual([
        kernel_cmdline.KeyValue('a', '', ''),
        kernel_cmdline.KeyValue('b', '=', 'f'),
        kernel_cmdline.KeyValue('c', '', '')], cmd.kern_args)

  def testSetKernelParameterAppends(self):
    """Test SetKernelParameter() appends new key"""
    cmd = kernel_cmdline.CommandLine('a b c')
    self.assertEqual(
        3, cmd.SetKernelParameter(kernel_cmdline.KeyValue('d', '=', '99')))
    self.assertEqual([
        kernel_cmdline.KeyValue('a', '', ''),
        kernel_cmdline.KeyValue('b', '', ''),
        kernel_cmdline.KeyValue('c', '', ''),
        kernel_cmdline.KeyValue('d', '=', '99')], cmd.kern_args)

  def testSetKernelParameterAddsQuotes(self):
    """Test that SetKernelParameter adds double-quotes when needed."""
    cmd = kernel_cmdline.CommandLine('a b c')
    self.assertEqual(
        3, cmd.SetKernelParameter(kernel_cmdline.KeyValue('d', '=', '9 9')))
    self.assertEqual([
        kernel_cmdline.KeyValue('a', '', ''),
        kernel_cmdline.KeyValue('b', '', ''),
        kernel_cmdline.KeyValue('c', '', ''),
        kernel_cmdline.KeyValue('d', '=', '"9 9"')], cmd.kern_args)

  def testFormat(self):
    """Test that the output is correct"""
    self.assertEqual(CMDLINE, kernel_cmdline.CommandLine(CMDLINE).Format())

  def testGetDmConfig(self):
    """Test that GetDmConfig returns the DmConfig we expect."""
    cmd = kernel_cmdline.CommandLine(CMDLINE)
    dm = kernel_cmdline.DmConfig(DM)
    self.assertEqual(dm, cmd.GetDmConfig())

  def testSetDmConfig(self):
    """Test that SetDmConfig sets the dm= parameter."""
    cmd = kernel_cmdline.CommandLine('a -- b')
    dm = kernel_cmdline.DmConfig(DM)
    cmd.SetDmConfig(dm)
    expected = kernel_cmdline.KeyValue('dm', '=', '"%s"' % DM)
    self.assertEqual(expected, cmd.kern_args[1])


class DmConfigTest(cros_test_lib.TestCase):
  """Test DmConfig."""

  def testParses(self):
    """Verify that DmConfig correctly parses the config from DmDevice."""
    device_data = [
        ['vboot uuid ro 1', '0 1 verity arg1 arg2'],
        ['vblah uuid2 ro 2',
         '100 9 stripe larg1=val1',
         '200 10 stripe larg2=val2'],
        ['vroot uuid3 ro 1', '99 3 linear larg1 larg2']]
    dm_arg = ', '.join(','.join(x for x in data) for data in device_data)
    devices = [kernel_cmdline.DmDevice(x) for x in device_data]
    dc = kernel_cmdline.DmConfig('%d %s' % (len(device_data), dm_arg))
    self.assertEqual(len(device_data), dc.num_devices)
    self.assertEqual(devices, list(dc.devices.values()))

  def testFormats(self):
    """Verify that DmConfig recreates its input string."""
    device_data = [
        ['vboot uuid ro 1', '0 1 verity arg1 arg2'],
        ['vblah uuid2 ro 2',
         '100 9 stripe larg1=val1',
         '200 10 stripe larg2=val2'],
        ['vroot uuid3 ro 1', '99 3 linear larg1 larg2']]
    dm_arg = ', '.join(','.join(x for x in data) for data in device_data)
    dm_config_val = '%d %s' % (len(device_data), dm_arg)
    dc = kernel_cmdline.DmConfig(dm_config_val)
    self.assertEqual(dm_config_val, dc.Format())

  def testEqual(self):
    """Test that equal instances are equal."""
    arg = '2 v1 u1 f1 1,0 1 t1 a1, v2 u2 f2 2,3 4 t2 a2,5 6 t3 a3'
    dc = kernel_cmdline.DmConfig(arg)
    self.assertEqual(dc, kernel_cmdline.DmConfig(arg))

  def testNotEqual(self):
    """Test that unequal instances are unequal."""
    # Start with duplicated instances, and change fields to verify that all the
    # fields matter.
    arg = '2 v1 u1 f1 1,0 1 t1 a1, v2 u2 f2 2,3 4 t2 a2,5 6 t3 a3'
    dc1 = kernel_cmdline.DmConfig(arg)
    self.assertNotEqual(dc1, '')
    dc2 = kernel_cmdline.DmConfig(arg)
    dc2.num_devices = 1
    self.assertNotEqual(dc1, dc2)
    dc3 = kernel_cmdline.DmConfig(arg)
    dc3.devices = []
    self.assertNotEqual(dc1, dc3)


class DmDeviceTest(cros_test_lib.TestCase):
  """Test DmDevice."""

  def testParsesOneLine(self):
    """Verify that DmDevice correctly handles the results from DmLine."""
    lines = ['vboot none ro 1', '0 1 verity arg']
    dd = kernel_cmdline.DmDevice(lines)
    self.assertEqual('vboot', dd.name)
    self.assertEqual('none', dd.uuid)
    self.assertEqual('ro', dd.flags)
    self.assertEqual(1, dd.num_rows)
    self.assertEqual([kernel_cmdline.DmLine('0 1 verity arg')], dd.rows)

  def testParsesMultiLine(self):
    """Verify that DmDevice correctly handles multiline device."""
    lines = ['vboot none ro 2', '0 1 verity arg', '9 10 type a2']
    dd = kernel_cmdline.DmDevice(lines)
    self.assertEqual('vboot', dd.name)
    self.assertEqual('none', dd.uuid)
    self.assertEqual('ro', dd.flags)
    self.assertEqual(2, dd.num_rows)
    self.assertEqual([kernel_cmdline.DmLine('0 1 verity arg'),
                      kernel_cmdline.DmLine('9 10 type a2')], dd.rows)

  def testParsesIgnoresExcessRows(self):
    """Verify that DmDevice ignores excess rows."""
    lines = ['vboot none ro 2', '0 1 verity arg', '9 10 type a2', '4 5 t a3']
    dd = kernel_cmdline.DmDevice(lines)
    self.assertEqual('vboot', dd.name)
    self.assertEqual('none', dd.uuid)
    self.assertEqual('ro', dd.flags)
    self.assertEqual(2, dd.num_rows)
    self.assertEqual([kernel_cmdline.DmLine('0 1 verity arg'),
                      kernel_cmdline.DmLine('9 10 type a2')], dd.rows)

  def testEqual(self):
    """Test that equal instances are equal."""
    dd = kernel_cmdline.DmDevice(['v u f 1', '0 1 typ a'])
    self.assertEqual(dd, kernel_cmdline.DmDevice(['v u f 1', '0 1 typ a']))

  def testMultiLineEqual(self):
    """Test that equal instances are equal."""
    dd = kernel_cmdline.DmDevice(['v u f 2', '0 1 typ a', '2 3 t b'])
    self.assertEqual(
        dd, kernel_cmdline.DmDevice(['v u f 2', '0 1 typ a', '2 3 t b']))

  def testNotEqual(self):
    """Test that unequal instances are unequal."""
    dd = kernel_cmdline.DmDevice(['v u f 2', '0 1 typ a', '2 3 t b'])
    self.assertNotEqual(dd, '')
    self.assertNotEqual(
        dd, kernel_cmdline.DmDevice(['x u f 2', '0 1 typ a', '2 3 t b']))
    self.assertNotEqual(
        dd, kernel_cmdline.DmDevice(['v x f 2', '0 1 typ a', '2 3 t b']))
    self.assertNotEqual(
        dd, kernel_cmdline.DmDevice(['v u x 2', '0 1 typ a', '2 3 t b']))
    self.assertNotEqual(
        dd, kernel_cmdline.DmDevice(['v u f 1', '0 1 typ a', '2 3 t b']))
    self.assertNotEqual(
        dd, kernel_cmdline.DmDevice(['v u f 2', '9 1 typ a', '2 3 t b']))


class DmLineTest(cros_test_lib.TestCase):
  """Test DmLine."""

  def testParses(self):
    """Verify that DmLine correctly parses a line, and returns it."""
    text = '0 1 verity a1 a2=v2 a3'
    dl = kernel_cmdline.DmLine(text)
    self.assertEqual(0, dl.start)
    self.assertEqual(1, dl.num)
    self.assertEqual('verity', dl.target_type)
    self.assertEqual(kernel_cmdline._ParseKeyValue('a1 a2=v2 a3'), dl.args)
    self.assertEqual(text, dl.Format())

  def testAllowsWhitespace(self):
    """Verify that leading/trailing whitespace is ignored."""
    text = '0 1 verity a1 a2=v2 a3'
    dl = kernel_cmdline.DmLine(' %s ' % text)
    self.assertEqual(0, dl.start)
    self.assertEqual(1, dl.num)
    self.assertEqual('verity', dl.target_type)
    self.assertEqual(kernel_cmdline._ParseKeyValue('a1 a2=v2 a3'), dl.args)
    self.assertEqual(text, dl.Format())

  def testEqual(self):
    """Test that equal instances are equal."""
    dl = kernel_cmdline.DmLine('0 1 verity a1 a2=v2 a3')
    self.assertEqual(dl, kernel_cmdline.DmLine('0 1 verity a1 a2=v2 a3'))

  def testNotEqual(self):
    """Test that unequal instances are unequal."""
    dl = kernel_cmdline.DmLine('0 1 verity a1 a2=v2 a3')
    self.assertNotEqual(dl, '')
    self.assertNotEqual(dl, kernel_cmdline.DmLine('1 1 verity a1 a2=v2 a3'))
    self.assertNotEqual(dl, kernel_cmdline.DmLine('0 2 verity a1 a2=v2 a3'))
    self.assertNotEqual(dl, kernel_cmdline.DmLine('0 1 verit  a1 a2=v2 a3'))
    self.assertNotEqual(dl, kernel_cmdline.DmLine('0 1 verity aN a2=v2 a3'))
