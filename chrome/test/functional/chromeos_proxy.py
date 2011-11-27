#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional
import pyauto


class ChromeosProxy(pyauto.PyUITest):
  """Tests for ChromeOS proxy.

  The following tests are for UI testing to verify the user defined
  proxy settings can be set and saved.  The proxy addresses used in these
  tests are not real proxies and are used for testing purposes.
  """

  # The list of the default ports by protocol.  The default ports
  # for https and ftp are both 80 as explained in crosbug.com/14390#c3.
  DEFAULT_PORTS = {
    'http' : 80,
    'https': 80,
    'ftp'  : 80,
    'socks' : 1080,
  }

  # Defines the translation used for entering fields.
  MANUAL_PROXY_FIELDS = {
    'http'  : { 'url' : 'httpurl', 'port' : 'httpport' },
    'https' : { 'url' : 'httpsurl', 'port' : 'httpsport' },
    'ftp'   : { 'url' : 'ftpurl', 'port' : 'ftpport' },
    'socks' : { 'url' : 'socks', 'port' : 'socksport' },
  }

  def setUp(self):
    pyauto.PyUITest.setUp(self)
    self.SetProxySettingsOnChromeOS('type', self.PROXY_TYPE_DIRECT)

  def tearDown(self):
    self.SetProxySettingsOnChromeOS('type', self.PROXY_TYPE_DIRECT)
    pyauto.PyUITest.tearDown(self)

  def _BasicSetManualProxyFieldTest(self, proxy_type, proxy_url,
                                    proxy_port=None):
    """Basic test for setting one manual port field and verifying it is saved

    Args:
      proxy_type: One of http, https, ftp, or socks.
      proxy_url: The URL of the proxy server.
      proxy_port: The port number.  May be left blank to imply using the default
                  port.  The default ports are defined by self.DEFAULT_PORTS.
    """
    field = ChromeosProxy.MANUAL_PROXY_FIELDS[proxy_type]
    self.SetProxySettingsOnChromeOS(field['url'], proxy_url)
    if proxy_port is not None:
      self.SetProxySettingsOnChromeOS(field['port'], proxy_port)
    result = self.GetProxySettingsOnChromeOS()

    self.assertEqual(result['type'], self.PROXY_TYPE_MANUAL, 'Proxy type '
                     'should be Manual but instead we got %s.' %
                     self.GetProxyTypeName(result['type']))

    self.assertTrue(field['url'] in result,
                    'No %s url entry was saved' % proxy_type)
    self.assertEqual(result[field['url']], proxy_url,
                     'Saved proxy url %s does not match user set url %s.' %
                     (result[field['url']], proxy_url))
    # Verify the port. If proxy_port is empty, we verify the
    # default port is used
    if proxy_port is None:
      self.assertEqual(ChromeosProxy.DEFAULT_PORTS[proxy_type],
                       result[field['port']],
                       'Proxy port %d was used instead of default port %d.' %
                       (result[field['port']],
                        ChromeosProxy.DEFAULT_PORTS[proxy_type]))
    else:
      self.assertEqual(proxy_port, result[field['port']],
                       'Proxy port %d was used instead of user set port %d.' %
                       (result[field['port']], proxy_port))

    # Verify that all other proxy fields are not set.
    for key, val in self.MANUAL_PROXY_FIELDS.iteritems():
      if proxy_type != key:
        self.assertFalse(val['url'] in result, 'Only %s url should have '
                         'been set. %s url should have been left blank.' %
                         (proxy_type, key))

        self.assertFalse(val['port'] in result, 'Only %s port should have '
                         'been set. %s port should have been left blank.' %
                         (proxy_type, key))

  def testSetHTTPProxySettingsDefaultPort(self):
    """Set the http proxy without a port and verify it saves.

    If http proxy is provided but the port is not, the default port
    should be used.
    """
    self._BasicSetManualProxyFieldTest('http', '192.168.1.1')

  def testSetHTTPProxySettingsDefaultPortByDomain(self):
    """Set the http proxy by domain without a port and verify it saves.

    If http proxy is provided but the port is not, the default port
    should be used.
    """
    self._BasicSetManualProxyFieldTest('http', 'test_proxy.google.com')

  def testSetHTTPSProxySettingsDefaultPort(self):
    """Set https proxy without a port and verify it saves.

    If https proxy is provided but the port is not, the default port
    should be used.
    """
    self._BasicSetManualProxyFieldTest('https', '192.168.1.1')

  def testSetHTTPSProxySettingsDefaultPortByDomain(self):
    """Set the https proxy by domain without a port and verify it saves.

    If https proxy is provided but the port is not, the default port
    should be used.
    """
    self._BasicSetManualProxyFieldTest('https', 'test_proxy.google.com')

  def testSetFTPProxySettingsDefaultPort(self):
    """Set ftp proxy without a port and verify it saves.

    If ftp proxy is provided but the port is not, the default port
    should be used.
    """
    self._BasicSetManualProxyFieldTest('ftp', '192.168.1.1')

  def testSetFTPSProxySettingsDefaultPortByDomain(self):
    """Set the ftp proxy by domain without a port and verify it saves.

    If ftp proxy is provided but the port is not, the default port
    should be used.
    """
    self._BasicSetManualProxyFieldTest('ftp', 'test_proxy.google.com')

  def testSetSocksProxySettingsDefaultPort(self):
    """Set socks proxy without a port and verify it saves.

    If socks proxy is provided but the port is not, the default port
    should be used.
    """
    self._BasicSetManualProxyFieldTest('socks', '192.168.1.1')

  def testSetSocksProxySettingsDefaultPortByDomain(self):
    """Set socks proxy without a port and verify it saves.

    If socks proxy is provided but the port is not, the default port
    should be used.
    """
    self._BasicSetManualProxyFieldTest('socks', 'test_proxy.google.com')

  def testSetHTTPProxySettings(self):
    """Set the http proxy and verify it saves."""
    self._BasicSetManualProxyFieldTest('http', '192.168.1.1', 3128)

  def testSetHTTPProxySettingsByDomain(self):
    """Set the http proxy by domain name and verify it saves."""
    self._BasicSetManualProxyFieldTest('http', 'test_proxy.google.com', 3128)

  def testSetHTTPSProxySettings(self):
    """Set the https proxy and verify it saves."""
    self._BasicSetManualProxyFieldTest('https', '192.168.1.1', 6000)

  def testSetHTTPSProxySettingsByDomain(self):
    """Set the https proxy by domain name and verify it saves."""
    self._BasicSetManualProxyFieldTest('https', 'test_proxy.google.com', 6000)

  def testSetFtpProxySettingsByDomain(self):
    """Set the ftpproxy by domain name and verify it saves."""
    self._BasicSetManualProxyFieldTest('ftp', 'test_proxy.google.com', 7000)

  def testSetFTPProxySettings(self):
    """Set the ftp proxy and verify it saves."""
    self._BasicSetManualProxyFieldTest('ftp', '192.168.1.1', 7000)

  def testSetSocksProxySettings(self):
    """Set the socks proxy and verify it saves."""
    self._BasicSetManualProxyFieldTest('socks', '192.168.1.1', 3128)

  def testSetSocksProxySettingsByDomain(self):
    """Set the Socks proxy by domain name and verify it saves."""
    self._BasicSetManualProxyFieldTest('socks', 'test_proxy.google.com', 3128)


if __name__ == '__main__':
  pyauto_functional.Main()
