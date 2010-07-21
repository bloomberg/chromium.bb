#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re

import pyauto_functional  # Must be imported before pyauto
import pyauto


class InfobarTest(pyauto.PyUITest):
  """TestCase for Infobars."""

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    To run:
      python chrome/test/functional/infobars.py infobars.InfobarTest.Debug
    """
    import pprint
    pp = pprint.PrettyPrinter(indent=2)
    while True:
      raw_input('Hit <enter> to dump info.. ')
      info = self.GetBrowserInfo()
      for window in info['windows']:
        for tab in window['tabs']:
          print 'Window', window['index'], 'tab', tab['index']
          pp.pprint(tab['infobars'])

  def testPluginCrashInfobar(self):
    """Verify the "plugin crashed" infobar."""
    flash_url = self.GetFileURLForPath(os.path.join(self.DataDir(),
                                                    'plugin', 'flash.swf'))
    # Trigger flash plugin
    self.NavigateToURL(flash_url)
    child_processes = self.GetBrowserInfo()['child_processes']
    flash = [x for x in child_processes if
             x['type'] == 'Plug-in' and x['name'] == 'Shockwave Flash'][0]
    self.assertTrue(flash)
    logging.info('Killing flash plugin. pid %d' % flash['pid'])
    self.Kill(flash['pid'])
    self.WaitForInfobarCount(1)
    crash_infobar = self.GetBrowserInfo()['windows'][0]['tabs'][0]['infobars']
    self.assertTrue(crash_infobar)
    self.assertEqual(1, len(crash_infobar))
    self.assertTrue(re.match('The following plug-in has crashed:',
                             crash_infobar[0]['text']))


if __name__ == '__main__':
  pyauto_functional.Main()
