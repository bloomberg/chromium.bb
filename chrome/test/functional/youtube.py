#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time

import pyauto_functional
import pyauto


class YoutubeTest(pyauto.PyUITest):
  """Test case for Youtube videos"""

  # YouTube player states
  is_unstarted = '-1'
  is_playing = '1'
  is_paused = '2'
  has_ended = '0'

  def _IsFlashPluginEnabled(self):
    """Verify flash plugin availability and its state"""
    return [x for x in self.GetPluginsInfo().Plugins() \
               if x['name'] == 'Shockwave Flash' and x['enabled']]

  def _WaitUntilPlayerReady(self):
    """Verify that player is ready"""
    return self.WaitUntil(lambda: self.ExecuteJavascript("""
        player_status = document.getElementById("player_status");
        window.domAutomationController.send(player_status.innerHTML);
    """), expect_retval='player ready')

  def _GetPlayerState(self):
    """Returns a player state"""
    js = """
        var val = ytplayer.getPlayerState(); 
        window.domAutomationController.send(val + '');
    """
    return self.ExecuteJavascript(js)

  def _GetVideoTotalBytes(self):
    """Returns video size in bytes
       
    To call this function, video must be in the paying state,
    or this returns 0.
    """
    total_bytes = 0
    total_bytes = self.ExecuteJavascript("""
        bytes = ytplayer.getVideoBytesTotal();
        window.domAutomationController.send(bytes + '');
    """)
    return int(total_bytes)

  def _PlayVideo(self):
    """Plays the loaded video"""
    self.ExecuteJavascript("""
        ytplayer.playVideo();
        window.domAutomationController.send('');
    """)

  def _AssertPlayingState(self):
    """Assert player's playing state"""
    self.assertTrue(self.WaitUntil(self._GetPlayerState,
                        expect_retval=self.is_playing),
                        msg='Player did not enter the playing state')

  def _PlayVideoAndAssert(self):
    """Start video and assert the playing state"""
    self.assertTrue(self._IsFlashPluginEnabled(), 
                    msg='From here Flash plugin is disabled or not available') 
    url = self.GetHttpURLForDataPath('media', 'youtube.html')
    self.NavigateToURL(url)
    self.assertTrue(self._WaitUntilPlayerReady(),
                    msg='Failed to load YouTube player')
    self._PlayVideo()
    self._AssertPlayingState()

  def testPlayerStatus(self):
    """Test that YouTube loads a player and changes player states 
        
    Test verifies various player states like unstarted, playing, paused
    and ended.
    """
    # Navigating to Youtube video. This video is 122 seconds long.
    # During tests, we are not goinig to play this video full.
    self._PlayVideoAndAssert()
    # Pause the playing video
    self.ExecuteJavascript("""
        ytplayer.pauseVideo();
        window.domAutomationController.send('');
    """)
    self.assertEqual(self._GetPlayerState(), self.is_paused,
                     msg='Player did not enter the paused state')
    # Seek to the end of video
    self.ExecuteJavascript("""
        val = ytplayer.getDuration();
        ytplayer.seekTo(val, true);
        window.domAutomationController.send('');
    """)
    self._PlayVideo()
    # We've seeked to almost the end of the video but not quite.
    # Wait until the end.
    self.assertTrue(self.WaitUntil(self._GetPlayerState,
                                   expect_retval=self.has_ended), 
                    msg='Player did not reach the stopped state')

  def testPlayerResolution(self):
    """Test various video resolutions"""
    self._PlayVideoAndAssert()
    resolutions = self.ExecuteJavascript("""
        res = ytplayer.getAvailableQualityLevels();
        window.domAutomationController.send(res.toString());
    """)
    resolutions = resolutions.split(',')
    for res in resolutions:
      self.ExecuteJavascript("""
          ytplayer.setPlaybackQuality('%s');
          window.domAutomationController.send('');
      """ % res)
      curr_res = self.ExecuteJavascript("""
          res = ytplayer.getPlaybackQuality();
          window.domAutomationController.send(res + '');
      """)
      self.assertEqual(res, curr_res, msg='Resolution is not set to %s.' % res)
      
  def testPlayerBytes(self):
    """Test that player downloads video bytes"""
    self._PlayVideoAndAssert()
    total_bytes = self._GetVideoTotalBytes()
    prev_loaded_bytes = 0
    loaded_bytes = 0
    count = 0
    while loaded_bytes < total_bytes:
      # We want to test bytes loading only twice
      count = count + 1
      if count == 2:
        break
      loaded_bytes = self.ExecuteJavascript("""
          bytes = ytplayer.getVideoBytesLoaded();
          window.domAutomationController.send(bytes + '');
      """)
      loaded_bytes = int(loaded_bytes)
      self.assertTrue(prev_loaded_bytes <= loaded_bytes)
      prev_loaded_bytes = loaded_bytes
      # Give some time to load a video
      time.sleep(1)
 

if __name__ == '__main__':
  pyauto_functional.Main()
