# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import dlink_ap_configurator
import linksys_ap_configurator

import pyauto_ap_configurator  # must preceed pyauto
import pyauto


class APConfiguratorFactory(object):
  """Class that instantiates all available APConfigurators."""

  def __init__(self, pyauto_instance, config_dict_file_path=None):
    if not config_dict_file_path:
      # Load the default dictionary file
      config_dict_file_path = os.path.join(pyauto_instance.DataDir(),
                                           'pyauto_private', 'chromeos',
                                           'network', 'wifi_compat_config')
    assert os.path.exists(config_dict_file_path), ('%s missing' %
                                                   config_dict_file_path)
    ap_dict = pyauto_instance.EvalDataFrom(config_dict_file_path)
    self.ap_list = []
    self.ap_list.append(linksys_ap_configurator.LinksysAPConfigurator(
        pyauto_instance, ap_dict['LinksysAPConfigurator']))
    self.ap_list.append(dlink_ap_configurator.DLinkAPConfigurator(
        pyauto_instance, ap_dict['DLinkAPConfigurator']))

  def GetAPConfigurators(self):
    return self.ap_list

  def GetAPConfiguratorByShortName(self, name):
    for ap in self.ap_list:
      if ap.GetRouterShortName() == name:
        return ap
    return None
