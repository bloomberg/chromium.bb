# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

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
    tn = telnetlib.Telnet(self._host)
    tn.read_until('Username: ')
    tn.write(self._user + '\n')
    tn.read_until('Password: ')
    tn.write(self._password + '\n')
    tn.read_until('Switched CDU: ')
    tn.write('%s %s\n' % (command, outlet))
    tn.read_some()
    tn.close()
