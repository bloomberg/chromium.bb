# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional  # must be imported before pyauto
import pyauto


class PyAutoEventsTest(pyauto.PyUITest):
  """Tests using the event queue."""

  def testBasicEvents(self):
    """Basic test for the event queue."""
    url = self.GetHttpURLForDataPath('apptest', 'basic.html')
    driver = self.NewWebDriver()
    event_id = self.AddDomRaisedEventObserver(automation_id=4444);
    success_id = self.AddDomRaisedEventObserver('test success',
                                                automation_id=4444);
    self.NavigateToURL(url)
    self._ExpectEvent(event_id, 'init')
    self._ExpectEvent(event_id, 'login ready')
    driver.find_element_by_id('login').click()
    self._ExpectEvent(event_id, 'login start')
    self._ExpectEvent(event_id, 'login done')
    self.GetNextEvent(success_id)

  def _ExpectEvent(self, event_id, event_name):
    """Checks that the next event is expected."""
    e = self.GetNextEvent(event_id)
    self.assertEqual(e.get('name'), event_name,
                     msg="unexpected event: %s" % e)


if __name__ == '__main__':
  pyauto_functional.Main()
