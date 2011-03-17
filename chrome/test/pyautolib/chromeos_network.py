#!/usr/bin/python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import dbus
import pyauto


class PyNetworkUITest(pyauto.PyUITest):
  """A subclass of PyUITest for Chrome OS network tests.

  A subclass of PyUITest that automatically sets the flimflam
  priorities to put wifi connections first before starting tests.
  This is for convenience when writing wifi tests.
  """

  _FLIMFLAM_PATH = 'org.chromium.flimflam'
  _proxy = dbus.SystemBus().get_object(_FLIMFLAM_PATH, '/')
  _manager = dbus.Interface(_proxy, _FLIMFLAM_PATH + '.Manager')

  def setUp(self):
    # Move ethernet to the end of the flimflam priority list,
    # effectively hiding any ssh connections that the
    # test harness might be using and putting wifi ahead.
    self._SetServiceOrder('vpn,bluetooth,wifi,wimax,cellular,ethernet')
    pyauto.PyUITest.setUp(self)

  def tearDown(self):
    pyauto.PyUITest.tearDown(self)
    self._ResetServiceOrder()

  def _SetServiceOrder(self, service_order):
    self._old_service_order = self._manager.GetServiceOrder()
    self._manager.SetServiceOrder(service_order)
    assert service_order == self._manager.GetServiceOrder(), \
        'Flimflam service order not set properly.'

  def _ResetServiceOrder(self):
    self._manager.SetServiceOrder(self._old_service_order)
    assert self._old_service_order == self._manager.GetServiceOrder(), \
        'Flimflam service order not reset properly.'
