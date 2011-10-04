#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time

import pyauto_functional
import pyauto


class NetflixTest(pyauto.PyUITest):
  """Test case for Netflix player"""

  # Netflix player states
  is_playing = '4'

  title_homepage = 'http://movies.netflix.com/WiHome'
  signout_page = 'https://account.netflix.com/Logout'
  # 30 Rock
  test_title = 'http://movies.netflix.com/WiPlayer?'+ \
               'movieid=70136124&trkid=2361637&t=30+Rock'

  def tearDown(self):
    self._SignOut()
    pyauto.PyUITest.tearDown(self) 

  def _IsNetflixPluginEnabled(self):
    """Determine Netflix plugin availability and its state"""
    return [x for x in self.GetPluginsInfo().Plugins() \
               if x['name'] == 'Netflix' and x['enabled']]

  def _LoginToNetflix(self):
    """Login to Netflix"""
    credentials = self.GetPrivateInfo()['test_netflix_acct']
    self.NavigateToURL(credentials['login_url'])
    login_js = """
        document.getElementById('email').value='%s';
        document.getElementById('password').value='%s';
        document.getElementById('login-form').submit()
        window.domAutomationController.send('ok');
    """ % (credentials['username'], credentials['password'])
    self.assertEqual(self.ExecuteJavascript(login_js), 'ok',
        msg='Login to Netflix failed')
    
  def _HandleInfobars(self):
    """Manage infobars, come up during the test.

    We expect password and Netflix infobars. Processing only Netflix infobar,
    since to start a vidoe, pressing the OK button is a must. We can keep other
    inforbars open."""
    self.WaitForInfobarCount(2)
    tab_info = self.GetBrowserInfo()['windows'][0]['tabs'][0]
    infobars = tab_info['infobars']
    index = 0
    netflix_infobar_status = False
    for infobar in infobars:
       if infobar['buttons'][0] == 'OK':
         self.PerformActionOnInfobar('accept', infobar_index=index)
         netflix_infobar_status = True
       index = index + 1
    self.assertTrue(netflix_infobar_status, 
                    msg='Netflix infobar did not show up')
         
  def _CurrentPlaybackTime(self):
    """Returns the current playback time in seconds"""
    time = self.ExecuteJavascript("""
        time = nrdp.video.currentTime;
        window.domAutomationController.send(time + '');
    """)
    return int(float(time))

  def _SignOut(self):
    """Sing out from Netflix Login"""
    self.NavigateToURL(self.signout_page)

  def _LoginAndStartPlaying(self):
    """Login and start playing the video"""
    self.assertTrue(self._IsNetflixPluginEnabled(), 
                    msg='Netflix plugin is disabled or not available') 
    self._LoginToNetflix()
    self.assertTrue(self.WaitUntil(
        lambda:self.GetActiveTabURL().spec(),
        expect_retval=self.title_homepage),
        msg='Login to Netflix failed')
    self.NavigateToURL(self.test_title)
    self._HandleInfobars()
    self.assertTrue(self.WaitUntil(
        lambda: self.ExecuteJavascript("""
            player_status = nrdp.video.readyState;
            window.domAutomationController.send(player_status + '');
        """), expect_retval=self.is_playing),
        msg='Player did not start playing the title')

  def testPlayerLoadsAndPlays(self):
    """Test that Netflix player loads and plays the title"""
    self._LoginAndStartPlaying()

  def testPlaying(self):
    """Test that title playing progresses"""
    self._LoginAndStartPlaying()
    title_length =  self.ExecuteJavascript("""
        time = nrdp.video.duration;
        window.domAutomationController.send(time + '');
    """)
    title_length = int(float(title_length))
    prev_time = 0
    current_time = 0
    count = 0
    while current_time < title_length:
      # We want to test playing only for five seconds
      count = count + 1
      if count == 5:
        break
      current_time = self._CurrentPlaybackTime()
      self.assertTrue(prev_time <= current_time,
          msg='Prev playing time %s is greater than current time %s'
          % (prev_time, current_time))
      prev_time = current_time
      # play video for some time 
      time.sleep(1)


if __name__ == '__main__':
  pyauto_functional.Main()
