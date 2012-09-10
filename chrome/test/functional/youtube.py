#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import time

import pyauto_functional
import pyauto
import pyauto_errors
import test_utils


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

  def AssertPlayerState(self, state, msg):
    expected_regex = '^%s$' % state
    self.WaitForDomNode('id("playerState")', expected_value=expected_regex,
                        msg=msg)

  def WaitUntilPlayerReady(self):
    """Verify that player is ready."""
    self.AssertPlayerState(state=self.is_unstarted,
                           msg='Failed to load youtube player.')

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
        bytesTotal = document.getElementById("bytesTotal");
        window.domAutomationController.send(bytesTotal.innerHTML);
    """)
    return int(total_bytes)

  def GetVideoLoadedBytes(self):
    """Returns video size in bytes."""
    loaded_bytes = 0
    loaded_bytes = self.ExecuteJavascript("""
        bytesLoaded = document.getElementById("bytesLoaded");
        window.domAutomationController.send(bytesLoaded.innerHTML);
    """)
    return int(loaded_bytes)

  def GetCurrentVideoTime(self):
    """Returns the current time of the video in seconds."""
    current_time = 0
    current_time = self.ExecuteJavascript("""
        videoCurrentTime = document.getElementById("videoCurrentTime");
        window.domAutomationController.send(videoCurrentTime.innerHTML);
    """)
    return int(current_time)

  def PlayVideo(self):
    """Plays the loaded video."""
    self._pyauto.ExecuteJavascript("""
        ytplayer.playVideo();
        window.domAutomationController.send('');
    """)

  def StopVideo(self):
    """Stops the video and cancels loading."""
    self._pyauto.ExecuteJavascript("""
        ytplayer.stopVideo();
        window.domAutomationController.send('');
    """)

  def PauseVideo(self):
    """Pause the video."""
    self.ExecuteJavascript("""
        ytplayer.pauseVideo();
        window.domAutomationController.send('');
    """)

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
    self.WaitUntilPlayerReady()
    i = 0
    # The YouTube player will get in a state where it does not return the
    # number of loaded bytes.  When this happens we need to reload the page
    # before starting the test.
    while self.GetVideoLoadedBytes() == 1 and i < 30:
      self._pyauto.NavigateToURL(url)
      self.WaitUntilPlayerReady()
      i = i + 1
    self.PlayVideo()
    self.AssertPlayerState(state=self.is_playing,
                           msg='Player did not enter the playing state')

  def VideoBytesLoadingAndAssert(self):
    """Assert the video loading."""
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

  def PlayFAVideo(self):
    """Play and assert FA video playing."""
    credentials = self.GetPrivateInfo()['test_fa_account']
    test_utils.GoogleAccountsLogin(self,
        credentials['username'], credentials['password'])
    self.PlayVideoAndAssert('pmE6CJoq4Kg')


class YoutubeTest(pyauto.PyUITest, YoutubeTestHelper):
  """Test case for Youtube videos."""

  def __init__(self, methodName='runTest', **kwargs):
    pyauto.PyUITest.__init__(self, methodName, **kwargs)
    YoutubeTestHelper.__init__(self, self)

  def testPlayerStatus(self):
    """Test that YouTube loads a player and changes player states.

    Test verifies various player states like unstarted, playing, paused
    and ended.
    """
    # Navigating to Youtube video. This video is 122 seconds long.
    # During tests, we are not goinig to play this video full.
    self.PlayVideoAndAssert()
    self.PauseVideo()
    self.AssertPlayerState(state=self.is_paused,
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
    self.AssertPlayerState(state=self.has_ended,
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
    self.VideoBytesLoadingAndAssert()

  def testFAVideo(self):
    """Test that FlashAccess/DRM video plays."""
    self.PlayFAVideo()
    self.StopVideo()

  def testFAVideoBytes(self):
    """Test FlashAccess/DRM video bytes loading."""
    self.PlayFAVideo()
    self.VideoBytesLoadingAndAssert()
    self.StopVideo()


if __name__ == '__main__':
  pyauto_functional.Main()
