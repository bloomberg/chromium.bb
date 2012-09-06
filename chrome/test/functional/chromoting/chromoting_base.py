# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common imports, setup, etc for chromoting tests."""

import os


def _SetupPaths():
  """Add chrome/test/functional to sys.path for importing pyauto_functional"""
  functional_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
  os.sys.path.append(functional_dir)

_SetupPaths()


import pyauto_functional  # Must come before chromoting and pyauto.
from pyauto_functional import Main
import pyauto
import chromotinglib


class ChromotingBase(chromotinglib.ChromotingMixIn, pyauto.PyUITest):
  """Chromoting pyauto test base class.

  The following member variables can be used in the child classes:
    client_local: True if the client is on the same machines as host
    host: The chromoting host side, instance of ChromotingBase
    client: The chromoting client side, intance of ChromotingBase
    client_tab_index: The tab index to the chromoting client tab
  """
  def __init__(self, methodName):
    pyauto.PyUITest.__init__(self, methodName)

    self.client_local = (self.remote == None)
    self.host = self
    self.client = self if self.client_local else self.remote
    self.client_tab_index = 2 if self.client_local else 1

  def ExtraChromeFlags(self):
    """Add extra flags for chromoting testing

    Add --allow-nacl-socket-api to connect chromoting successfully.
    Add --allow-legacy-extension-manifests so that chrome can load
    chromoting webapp in v1 format.
    """
    extra_chrome_flags = [
        '--allow-nacl-socket-api=*',
        '--allow-legacy-extension-manifests'
        ]
    return pyauto.PyUITest.ExtraChromeFlags(self) + extra_chrome_flags