#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess as sub

import pyauto_functional  # Must be imported before pyauto
import pyauto


class ChromeosGSMCompliance(pyauto.PyUITest):
  """Tests for ChromeOS GSM compliance.

  Fakes connection to a network and verifies that the network name is correct.
  """

  _ethernet = None

  def _GetEthernet(self):
    """Get the ethernet to which the device is connected."""
    result = sub.Popen('ifconfig | cut -d\' \' -f 1 | grep eth',
                        stdout=sub.PIPE, shell=True).communicate()
    self.assertNotEqual(result[0], '', msg='Not connected to Ethernet.')
    self._ethernet = result[0].rstrip()

  def setUp(self):
    pyauto.PyUITest.setUp(self)
    sub.call(['sudo', 'stop', 'cromo'])
    self._GetEthernet()
    sub.call(['sudo', '/bin/sh', '/usr/local/lib/flimflam/test/backchannel',
              'setup', self._ethernet, 'pseudo-modem0'])

  def tearDown(self):
    if self._ethernet:
      sub.call(['sudo', '/bin/sh', '/usr/local/lib/flimflam/test/backchannel',
                'teardown', self._ethernet, 'pseudo-modem0'])
    pyauto.PyUITest.tearDown(self)

  def _IsFakeGSMRunning(self):
    """Check if fake-gsm-modem is running.

    Returns:
      True if fake-gsm-modem process is running and False if not.  
    """
    ps = sub.Popen('ps -ef | grep fake-gsm-modem | grep -v grep',
                     shell=True, stdout=sub.PIPE).communicate()
    return len(ps[0].split('fake-gsm-modem')) > 0

  def _VerifyBasicCarrierCompliance(self, carrier_name, cellular_name):
    """Faking the specified carrier and checking the name.

    Args:
      carrier_name: The name of the carrier.
      cellular_name: The name in the icon of the cellular network.
    """
    fake_gsm = sub.Popen(['sudo', '/usr/bin/python', 
                          '/usr/local/lib/flimflam/test/fake-gsm-modem',
                          '-c', carrier_name])
    self.assertTrue(self._IsFakeGSMRunning(), msg='Fake GSM is not running.')
    # Wait for the fake GSM to connect.
    cellular = 'cellular_networks'
    try:
      self.assertTrue(self.WaitUntil(lambda: cellular in
                      self.NetworkScan().keys(),timeout=10, retry_sleep=1))
    except AssertionError:
      fake_gsm.terminate()
      self.assertTrue(False, msg='No cellular networks appeared on scan.')
    network_info = self.NetworkScan()[cellular]
    result = any([network_info[key]['name'] == cellular_name
                  for key in network_info.keys()])
    fake_gsm.terminate()
    self.assertTrue(result, msg='The cellular network name did not match %s.'
                    % cellular_name)

  def testConnectThree(self):
    """Testing connection to Three."""
    self._VerifyBasicCarrierCompliance('three', '3')

  def testConnectSFR(self):
    """Testing connection to SFR."""
    self._VerifyBasicCarrierCompliance('SFR', 'SFR')

  def testConnectKPN(self):
    """Testing connection to KPN."""
    self._VerifyBasicCarrierCompliance('KPN', 'KPN NL')

  def testConnectSimyo(self):
    """Testing connection to Simyo."""
    self._VerifyBasicCarrierCompliance('Simyo', 'E-Plus')

  def testConnectMovistar(self):
    """Testing connection to Movistar."""
    self._VerifyBasicCarrierCompliance('Movistar', 'Movistar')

  def testConnectThreeita(self):
    """Testing connection to threeita."""
    self._VerifyBasicCarrierCompliance('threeita', '3ITA')

  def testConnectAtt(self):
    """Testing connection to att."""
    self._VerifyBasicCarrierCompliance('att', 'AT&T')

  def testConnectTmobile(self):
    """Testing connection to Tmobile."""
    self._VerifyBasicCarrierCompliance('Tmobile', 'T-Mobile')


if __name__ == '__main__':
  pyauto_functional.Main()
