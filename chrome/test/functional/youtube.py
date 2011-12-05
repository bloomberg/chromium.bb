#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import time

import pyauto_functional
import pyauto


class YoutubeTestHelper():
  """Helper functions for Youtube tests.
     
  For sample usage, look at class YoutubeTest.
  """

  # YouTube player states
  is_unstarted = '-1'
  is_playing = '1'
  is_paused = '2'
  has_ended = '0'
  _pyauto = None

  def __init__(self, pyauto):
    self._pyauto = pyauto

  def IsFlashPluginEnabled(self):
    """Verify flash plugin availability and its state."""
    return [x for x in self._pyauto.GetPluginsInfo().Plugins() \
            if x['name'] == 'Shockwave Flash' and x['enabled']]

  def WaitUntilPlayerReady(self):
    """Verify that player is ready."""
    return self._pyauto.WaitUntil(lambda: self._pyauto.ExecuteJavascript("""
        player_status = document.getElementById("player_status");
        window.domAutomationController.send(player_status.innerHTML);
    """), expect_retval='player ready')

  def GetPlayerState(self):
    """Returns a player state."""
    js = """
        var val = ytplayer.getPlayerState(); 
        window.domAutomationController.send(val + '');
    """
    return self._pyauto.ExecuteJavascript(js)

  def GetVideoInfo(self):
    """Returns Youtube video info."""
    youtube_apis = self._pyauto.GetPrivateInfo()['youtube_api']
    youtube_debug_text = youtube_apis['GetDebugText']
    return  self._pyauto.ExecuteJavascript(
        'window.domAutomationController.send(%s);' % youtube_debug_text)

  def GetVideoDroppedFrames(self):
    """Returns total Youtube video dropped frames.
  
    Returns:
      -1 if failed to get video frames from the video data
    """
    video_data = self._pyauto.GetVideoInfo()
    matched = re.search('droppedFrames=([\d\.]+)', video_data)
    if matched:
      return int(matched.group(1))
    else:
      return -1

  def GetVideoFrames(self):
    """Returns Youtube video frames/second.

    Returns:
      -1 if failed to get droppd frames from the video data.
    """
    video_data = self._pyauto.GetVideoInfo()
    matched = re.search('videoFps=([\d\.]+)', video_data)
    if matched:
      return int(matched.group(1))
    else:
      return -1

  def GetVideoTotalBytes(self):
    """Returns video total size in bytes.
       
    To call this function, video must be in the paying state,
    or this returns 0.
    """
    total_bytes = 0
    total_bytes = self._pyauto.ExecuteJavascript("""
        bytes = ytplayer.getVideoBytesTotal();
        window.domAutomationController.send(bytes + '');
    """)
    return int(total_bytes)

  def GetVideoLoadedBytes(self):
    """Returns video size in bytes."""
    loaded_bytes = 0
    loaded_bytes = self.ExecuteJavascript("""
        bytes = ytplayer.getVideoBytesLoaded();
        window.domAutomationController.send(bytes + '');
    """)
    return int(loaded_bytes)

  def PlayVideo(self):
    """Plays the loaded video."""
    self._pyauto.ExecuteJavascript("""
        ytplayer.playVideo();
        window.domAutomationController.send('');
    """)

  def PauseVideo(self):
    """Pause the video."""
    self.ExecuteJavascript("""
        ytplayer.pauseVideo();
        window.domAutomationController.send('');
    """)

  def AssertPlayingState(self):
    """Assert player's playing state."""
    self._pyauto.assertTrue(self._pyauto.WaitUntil(self._pyauto.GetPlayerState,
        expect_retval=self._pyauto.is_playing),
        msg='Player did not enter the playing state')

  def PlayVideoAndAssert(self, youtube_video='zuzaxlddWbk'):
    """Start video and assert the playing state.
    
    By default test uses http://www.youtube.com/watch?v=zuzaxlddWbki.

    Args:
      youtube_video: The string ID of the youtube video to play.
    """
    self._pyauto.assertTrue(self._pyauto.IsFlashPluginEnabled(), 
        msg='From here Flash plugin is disabled or not available')
    url = self._pyauto.GetHttpURLForDataPath(
        'media', 'youtube.html?video=' + youtube_video)
    self._pyauto.NavigateToURL(url)
    self._pyauto.assertTrue(self._pyauto.WaitUntilPlayerReady(),
                            msg='Failed to load YouTube player')
    self._pyauto.PlayVideo()
    self._pyauto.AssertPlayingState()


class YoutubeTest(pyauto.PyUITest, YoutubeTestHelper):
  """Test case for Youtube videos."""

  def __init__(self, methodName='runTest', **kwargs):
    pyauto.PyUITest.__init__(self, methodName, **kwargs)
    YoutubeTestHelper.__init__(self, self)

  def testPlayerStatus(self):
    """Test that YouTube loads a player and changes player states 
        
    Test verifies various player states like unstarted, playing, paused
    and ended.
    """
    # Navigating to Youtube video. This video is 122 seconds long.
    # During tests, we are not goinig to play this video full.
    self.PlayVideoAndAssert()
    self.PauseVideo()
    self.assertEqual(self.GetPlayerState(), self.is_paused,
                     msg='Player did not enter the paused state')
    # Seek to the end of video
    self.ExecuteJavascript("""
        val = ytplayer.getDuration();
        ytplayer.seekTo(val, true);
        window.domAutomationController.send('');
    """)
    self.PlayVideo()
    # We've seeked to almost the end of the video but not quite.
    # Wait until the end.
    self.assertTrue(self.WaitUntil(self.GetPlayerState,
                                   expect_retval=self.has_ended), 
                    msg='Player did not reach the stopped state')

  def testPlayerResolution(self):
    """Test various video resolutions."""
    self.PlayVideoAndAssert()
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
    """Test that player downloads video bytes."""
    self.PlayVideoAndAssert()
    total_bytes = self.GetVideoTotalBytes()
    prev_loaded_bytes = 0
    loaded_bytes = 0
    count = 0
    while loaded_bytes < total_bytes:
      # We want to test bytes loading only twice
      count = count + 1
      if count == 2:
        break
      loaded_bytes = self.GetVideoLoadedBytes()
      self.assertTrue(prev_loaded_bytes <= loaded_bytes)
      prev_loaded_bytes = loaded_bytes
      # Give some time to load a video
      time.sleep(1)


if __name__ == '__main__':
  pyauto_functional.Main()
