# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import re
import telnetlib


class PowerStrip(object):
  """Controls Server Technology CW-16V1-C20M switched CDUs.
  (Cabinet Power Distribution Unit)

  This class is used to control the CW-16V1-C20M unit which
  is a 16 port remote power strip.  The strip supports AC devices
  using 100-120V 50/60Hz input voltages.  The commands in this
  class are supported by switches that use Sentry Switched CDU Version 6.0g.

  Opens a new connection for every command.
  """

  TIMEOUT = 10

  def __init__(self, host, user='admn', password='admn'):
    self._host = host
    self._user = user
    self._password = password

  def PowerOff(self, outlet):
    """Powers off the device that is plugged into the specified outlet.

    Args:
      outlet: The outlet ID defined on the switch (eg. .a14).
    """
    self._DoCommand('off', outlet)

  def PowerOn(self, outlet):
    """Powers on the device that is plugged into the specified outlet.

    Args:
      outlet: The outlet ID defined on the switch (eg. .a14).
    """
    self._DoCommand('on', outlet)

  def _DoCommand(self, command, outlet):
    """Performs power strip commands on the specified outlet.

    Sample telnet interaction:
      Escape character is '^]'.

      Sentry Switched CDU Version 6.0g

      Username: admn
      Password: < password hidden from view >

      Location:

      Switched CDU: on .a1

         Outlet   Outlet                     Outlet      Control
         ID       Name                       Status      State

         .A1      TowerA_Outlet1             On          On

         Command successful

      Switched CDU: < cdu cmd >

    Args:
      command: A valid CW-16V1-C20M command that follows the format
               <command> <outlet>.
      outlet: The outlet ID defined on the switch (eg. .a14).
    """
    tn = telnetlib.Telnet()
    # To avoid 'Connection Reset by Peer: 104' exceptions when rapid calls
    # are made to the telnet server on the power strip, we retry executing
    # a command.
    retry = range(5)
    for attempt in retry:
      try:
        tn.open(self._host, timeout=PowerStrip.TIMEOUT)
        resp = tn.read_until('Username:', timeout=PowerStrip.TIMEOUT)
        assert 'Username' in resp, 'Username not found in response. (%s)' % resp
        tn.write(self._user + '\n')

        resp = tn.read_until('Password:', timeout=PowerStrip.TIMEOUT)
        assert 'Password' in resp, 'Password not found in response. (%s)' % resp
        tn.write(self._password + '\n')

        resp = tn.read_until('Switched CDU:', timeout=PowerStrip.TIMEOUT)
        assert 'Switched CDU' in resp, 'Standard prompt not found in ' \
                                         'response. (%s)' % resp
        tn.write('%s %s\n' % (command, outlet))

        # Obtain the output of command and make sure it matches with the action
        # we performed.
        # Sample valid output:
        #   .A1      TowerA_Outlet1             On          On
        resp = tn.read_until('Switched CDU:', timeout=PowerStrip.TIMEOUT)
        if not re.search('%s\s+\S+\s+%s\s+%s' % (outlet, command, command),
                                                 resp, re.I):
          raise Exception('Command \'%s\' execution failed. (%s)' %
                          (command, resp))

        # Exiting the telnet session cleanly significantly reduces the chance of
        # connection error on initiating the following telnet session.
        tn.write('exit\n')
        tn.read_all()

        # If we've gotten this far, there is no need to retry.
        break
      except Exception as e:
        logging.debug('Power strip retry on cmd "%s".  Reason: %s'
                       % (command, str(e)))
        if attempt == retry[-1]:
          raise Exception('Sentry Command "%s" failed.  '
                          'Reason: %s' % (command, str(e)))
      finally:
        tn.close()
