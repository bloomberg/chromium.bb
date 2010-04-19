#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional  # Must be imported before pyauto
import pyauto


class HistoryTest(pyauto.PyUITest):
  """TestCase for History."""

  def testBasic(self):
    url = 'http://www.google.com/'
    title = 'Google'
    self.NavigateToURL(url)

    history = self.GetHistoryInfo().History()
    self.assertEqual(1, len(history))
    self.assertEqual(title, history[0]['title'])
    self.assertEqual(url, history[0]['url'])

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    """
    while True:
      raw_input('Interact with the browser and hit <enter> to dump history.. ')
      print '*' * 20
      history = self.GetHistoryInfo().History()
      import pprint
      pp = pprint.PrettyPrinter(indent=2)
      pp.pprint(history)


if __name__ == '__main__':
  pyauto_functional.Main()
