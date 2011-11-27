# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Python representation for Chromium Plugins info.

This is the info available at about:plugins.
Obtain one of these from PyUITestSuite::GetPluginsInfo() call.

Example:
class MyTest(pyauto.PyUITest):
  def testBasic(self):
    info = self.GetPluginsInfo()  # fetch plugins snapshot
    print info.Plugins()

See more examples in chrome/test/functional/plugins.py.
"""

import simplejson as json

from pyauto_errors import JSONInterfaceError


class PluginsInfo(object):
  """Represent info for Chromium plugins.

  The info is represented as a list of dictionaries, one for each plugin.
  """
  def __init__(self, json_string):
    """Initialize a PluginsInfo from a json string.

    Args:
      json_string: a json string, as returned by a json ipc call for the
                   command 'GetPluginsInfo'

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    # JSON string prepared in GetPluginsInfo() in automation_provider.cc
    self.pluginsdict = json.loads(json_string)
    if self.pluginsdict.has_key('error'):
      raise JSONInterfaceError(self.pluginsdict['error'])

  def Plugins(self):
    """Get plugins.

    Returns:
      a list of plugins info
      Sample:
      [ { u'desc': u'Shockwave Flash 10.0 r45',
          u'enabled': True,
          u'mimeTypes': [ { u'description': u'Shockwave Flash',
                            u'fileExtensions': [u'swf'],
                            u'mimeType': u'application/x-shockwave-flash'},
                          { u'description': u'FutureSplash Player',
                            u'fileExtensions': [u'spl'],
                            u'mimeType': u'application/futuresplash'}],
          u'name': u'Shockwave Flash',
          u'path': u'/Library/Internet Plug-Ins/Flash Player.plugin',
          u'version': u'10.0.45.2'},
        { u'desc': u'Version 1.1.2.9282',
          u'enabled': True,
          u'mimeTypes': [ { u'description': u'Google voice and video chat',
                            u'fileExtensions': [u'googletalk'],
                            u'mimeType': u'application/googletalk'}],
          u'name': u'Google Talk NPAPI Plugin',
          u'path': u'/Library/Internet Plug-Ins/googletalkbrowserplugin.plugin',
          u'version': u'1.1.2.9282'},
        ...,
        ...,
      ]
    """
    return self.pluginsdict.get('plugins', [])

  def PluginForPath(self, path):
    """Get plugin info for the given plugin path.

    Returns:
      a dictionary of info for the plugin.
    """
    got = filter(lambda x: x['path'] == path, self.Plugins())
    if not got: return None
    return got[0]

  def PluginForName(self, name):
    """Get plugin info for the given name.

    There might be several plugins with the same name.

    Args:
      name: the name for which to look for.

    Returns:
      a list of info dictionaries for each plugin found with the given name.
    """
    return filter(lambda x: x['name'] == name, self.Plugins())

  def FirstPluginForName(self, name):
    """Get plugin info for the first plugin with the given name.

    This is useful in case there are multiple plugins for a name.

    Args:
      name: the name for which to look for.

    Returns:
      a plugin info dictionary
      None, if not found
    """
    all = self.PluginForName(name)
    if not all: return None
    return all[0]
