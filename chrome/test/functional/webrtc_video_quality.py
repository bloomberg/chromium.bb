#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys

import pyauto_functional
import pyauto
import pyauto_paths
import pyauto_utils
import webrtc_test_base

# If you change the port number, don't forget to modify video_extraction.js too.
_PYWEBSOCKET_PORT_NUMBER = '12221'

_HOME_ENV_NAME = 'HOMEPATH' if pyauto.PyUITest.IsWin() else 'HOME'
_WORKING_DIR = os.path.join(os.environ[_HOME_ENV_NAME], 'webrtc_video_quality')

# This is the reference file that is being played by the virtual web camera.
_REFERENCE_YUV_FILE = os.path.join(_WORKING_DIR, 'reference_video.yuv')

# The YUV file is the file produced by rgba_to_i420_converter.
_OUTPUT_YUV_FILE = os.path.join(_WORKING_DIR, 'captured_video.yuv')


class MissingRequiredToolException(Exception):
  pass


class FailedToRunToolException(Exception):
  pass


class WebrtcVideoQualityTest(webrtc_test_base.WebrtcTestBase):
  """Test the video quality of the WebRTC output.

  Prerequisites: This test case must run on a machine with a virtual webcam that
  plays video from the reference file located in the location defined by
  _REFERENCE_YUV_FILE. You must also compile the chromium_builder_webrtc target
  before you run this test to get all the tools built.
  The external compare_videos.py script also depends on two external executables
  which must be located in the PATH when running this test.
  * zxing (see the CPP version at https://code.google.com/p/zxing)
  * ffmpeg 0.11.1 or compatible version (see http://www.ffmpeg.org)

  The test case will launch a custom binary (peerconnection_server) which will
  allow two WebRTC clients to find each other.

  The test also runs several other custom binaries - rgba_to_i420 converter and
  frame_analyzer. Both tools can be found under third_party/webrtc/tools. The
  test also runs a stand alone Python implementation of a WebSocket server
  (pywebsocket) and a barcode_decoder script.
  """

  def setUp(self):
    pyauto.PyUITest.setUp(self)
    if not os.path.exists(_WORKING_DIR):
      self.fail('Cannot find the working directory for the reference video and '
                'the temporary files: %s' % _WORKING_DIR)
    if not os.path.exists(_REFERENCE_YUV_FILE):
      self.fail('Cannot find the reference file to be used for video quality '
                'comparison: %s' % _REFERENCE_YUV_FILE)
    self.StartPeerConnectionServer()

  def tearDown(self):
    self._StopPywebsocketServer()
    self.StopPeerConnectionServer()

    pyauto.PyUITest.tearDown(self)
    self.assertEquals('', self.CheckErrorsAndCrashes())

  def _WebRtcCallWithHelperPage(self, test_page, helper_page):

    """Tests we can call, let run for some time and hang up with WebRTC.

    This test exercises pretty much the whole happy-case for the WebRTC
    JavaScript API. Currently, it exercises a normal call setup using the API
    defined at http://dev.w3.org/2011/webrtc/editor/webrtc.html. The API is
    still evolving.

    The test will load the supplied HTML file, which in turn will load different
    javascript files depending on which version of the signaling protocol
    we are running.
    The supplied HTML files will be loaded in two tabs and tell the web
    pages to start up WebRTC, which will acquire video and audio devices on the
    system. This will launch a dialog in Chrome which we click past using the
    automation controller. Then, we will order both tabs to connect the server,
    which will make the two tabs aware of each other. Once that is done we order
    one tab to call the other.

    We make sure that the javascript tells us that the call succeeded, lets it
    run for some time and try to hang up the call after that. While the call is
    running, we capture frames with the help of the functions in the
    video_extraction.js file.

    Args:
      test_page(string): The name of the test HTML page. It is looked for in the
        webrtc directory under chrome/test/data.
      helper_page(string): The name of the helper HTML page. It is looked for in
        the same directory as the test_page.
    """
    assert helper_page
    url = self.GetFileURLForDataPath('webrtc', test_page)
    helper_url = self.GetFileURLForDataPath('webrtc', helper_page)

    # Start the helper page in the first tab
    self.NavigateToURL(helper_url)

    # Start the test page in the second page.
    self.AppendTab(pyauto.GURL(url))

    self.assertEquals('ok-got-stream', self.GetUserMedia(tab_index=0))
    self.assertEquals('ok-got-stream', self.GetUserMedia(tab_index=1))
    self.Connect('user_1', tab_index=0)
    self.Connect('user_2', tab_index=1)

    self.CreatePeerConnection(tab_index=0)
    self.AddUserMediaLocalStream(tab_index=0)
    self.EstablishCall(from_tab_with_index=0, to_tab_with_index=1)

    # Wait for JavaScript to capture all the frames. In the HTML file we specify
    # how many seconds to capture frames.
    done_capturing = self.WaitUntil(
        function=lambda: self.ExecuteJavascript('doneFrameCapturing()',
                                                tab_index=1),
        expect_retval='done-capturing', retry_sleep=1.0,
        # TODO(phoglund): Temporary fix; remove after 2013-04-01
        timeout=90)

    self.assertTrue(done_capturing,
                    msg='Timed out while waiting frames to be captured.')

    # The hang-up will automatically propagate to the second tab.
    self.HangUp(from_tab_with_index=0)
    self.WaitUntilHangUpVerified(tab_index=1)

    self.Disconnect(tab_index=0)
    self.Disconnect(tab_index=1)

    # Ensure we didn't miss any errors.
    self.AssertNoFailures(tab_index=0)
    self.AssertNoFailures(tab_index=1)

  def testVgaVideoQuality(self):
    """Tests the WebRTC video output for a VGA video input.

    On the bots we will be running fake webcam driver and we will feed a video
    with overlaid barcodes. In order to run the analysis on the output, we need
    to use the original input video as a reference video.
    """
    helper_page = webrtc_test_base.WebrtcTestBase.DEFAULT_TEST_PAGE
    self._StartVideoQualityTest(test_page='webrtc_video_quality_test.html',
                                helper_page=helper_page,
                                reference_yuv=_REFERENCE_YUV_FILE, width=640,
                                height=480)

  def _StartVideoQualityTest(self, reference_yuv,
                             test_page='webrtc_video_quality_test.html',
                             helper_page='webrtc_jsep01_test.html',
                             width=640, height=480):
    """Captures video output into a canvas and sends it to a server.

    This test captures the output frames of a WebRTC connection to a canvas and
    later sends them over WebSocket to a WebSocket server implemented in Python.
    At the server side we can store the frames for subsequent quality analysis.

    After the frames are sent to the pywebsocket server, we run the RGBA to I420
    converter, the barcode decoder and finally the frame analyzer. We also print
    everything to the Perf Graph for visualization

    Args:
      reference_yuv(string): The name of the reference YUV video that will be
        used in the analysis.
      test_page(string): The name of the test HTML page. To be looked for in the
        webrtc directory under chrome/test/data.
      helper_page(string): The name of the HTML helper page. To be looked for in
        the same directory as the test_page.
      width(int): The width of the test video frames.
      height(int): The height of the test video frames.
    """
    self._StartPywebsocketServer()

    self._WebRtcCallWithHelperPage(test_page, helper_page)

    # Wait for JavaScript to send all the frames to the server. The test will
    # have quite a lot of frames to send, so it will take at least several
    # seconds.
    no_more_frames = self.WaitUntil(
        function=lambda: self.ExecuteJavascript('haveMoreFramesToSend()',
                                                tab_index=1),
        expect_retval='no-more-frames', retry_sleep=1, timeout=150)
    self.assertTrue(no_more_frames,
                    msg='Timed out while waiting for frames to send.')

    self.assertTrue(self._RunRGBAToI420Converter(width, height))

    stats_file = os.path.join(_WORKING_DIR, 'pyauto_stats.txt')
    analysis_result = self._CompareVideos(width, height, _OUTPUT_YUV_FILE,
                                          reference_yuv, stats_file)
    self._ProcessPsnrAndSsimOutput(analysis_result)
    self._ProcessFramesCountOutput(analysis_result)

  def _StartPywebsocketServer(self):
    """Starts the pywebsocket server."""
    print 'Starting pywebsocket server.'

    # Pywebsocket source directory.
    path_pyws_dir = os.path.join(pyauto_paths.GetThirdPartyDir(), 'pywebsocket',
                                 'src')

    # Pywebsocket standalone server.
    path_to_pywebsocket= os.path.join(path_pyws_dir, 'mod_pywebsocket',
                                      'standalone.py')

    # Path to the data handler to handle data received by the server.
    path_to_handler = os.path.join(pyauto_paths.GetSourceDir(), 'chrome',
                                   'test', 'functional')

    # The python interpreter binary.
    python_interp = sys.executable

    # The pywebsocket start command - we could add --log-level=debug for debug.
    # -p stands for port, -d stands for root_directory (where the data handlers
    # are).
    start_cmd = [python_interp, path_to_pywebsocket,
                 '-p', _PYWEBSOCKET_PORT_NUMBER,
                 '-d', path_to_handler,]
    env = os.environ
    # Set PYTHONPATH to include the pywebsocket base directory.
    env['PYTHONPATH'] = (path_pyws_dir + os.path.pathsep +
                         env.get('PYTHONPATH', ''))

    # Start the pywebsocket server. The server will not start instantly, so the
    # code opening websockets to it should take this into account.
    self._pywebsocket_server = subprocess.Popen(start_cmd, env=env)

  def _StopPywebsocketServer(self):
    """Stops the running instance of pywebsocket server."""
    print 'Stopping pywebsocket server.'
    if self._pywebsocket_server:
      self._pywebsocket_server.kill()

  def _RunRGBAToI420Converter(self, width, height):
    """Runs the RGBA to I420 converter.

    The rgba_to_i420_converter is part of the webrtc_test_tools target which
    should be build prior to running this test. The resulting binary should live
    next to Chrome.

    Args:
      width(int): The width of the frames to be converted and stitched together.
      height(int): The height of the frames to be converted and stitched.

    Returns:
      (bool): True if the conversion is successful, false otherwise.
    """
    path_to_rgba_converter = os.path.join(self.BrowserPath(),
                                          'rgba_to_i420_converter')
    path_to_rgba_converter = os.path.abspath(path_to_rgba_converter)
    path_to_rgba_converter = self.BinPathForPlatform(path_to_rgba_converter)

    if not os.path.exists(path_to_rgba_converter):
      raise webrtc_test_base.MissingRequiredBinaryException(
          'Could not locate rgba_to_i420_converter! Did you build the '
          'webrtc_test_tools target?')

    # We produce an output file that will later be used as an input to the
    # barcode decoder and frame analyzer tools.
    start_cmd = [path_to_rgba_converter, '--frames_dir=%s' % _WORKING_DIR,
                 '--output_file=%s' % _OUTPUT_YUV_FILE, '--width=%d' % width,
                 '--height=%d' % height, '--delete_frames']
    print 'Start command: ', ' '.join(start_cmd)
    rgba_converter = subprocess.Popen(start_cmd, stdout=sys.stdout,
                                      stderr=sys.stderr)
    rgba_converter.wait()
    return rgba_converter.returncode == 0

  def _CompareVideos(self, width, height, captured_video_filename,
                     reference_video_filename, stats_filename):
    """Compares the captured video with the reference video.

    The barcode decoder decodes the captured video containing barcodes overlaid
    into every frame of the video (produced by rgba_to_i420_converter). It
    produces a set of PNG images and a stats file that describes the relation
    between the filenames and the (decoded) frame number of each frame.

    Args:
      width(int): The frames width of the video to be decoded.
      height(int): The frames height of the video to be decoded.
      captured_video_filename(string): The captured video file we want to
        extract frame images and decode frame numbers from.
      reference_video_filename(string): The reference video file we want to
        compare the captured video quality with.
      stats_filename(string): Filename for the output file containing
        data that shows the relation between each frame filename and the
        reference file's frame numbers.

    Returns:
      (string): The output of the script.

    Raises:
      FailedToRunToolException: If the script fails to run.
    """
    path_to_analyzer = os.path.join(self.BrowserPath(), 'frame_analyzer')
    path_to_analyzer = os.path.abspath(path_to_analyzer)
    path_to_analyzer = self.BinPathForPlatform(path_to_analyzer)

    path_to_compare_script = os.path.join(pyauto_paths.GetThirdPartyDir(),
                                          'webrtc', 'tools',
                                          'compare_videos.py')
    if not os.path.exists(path_to_compare_script):
      raise MissingRequiredToolException('Cannot find the script at %s' %
                                         path_to_compare_script)
    python_interp = sys.executable
    cmd = [
      python_interp,
      path_to_compare_script,
      '--ref_video=%s' % reference_video_filename,
      '--test_video=%s' % captured_video_filename,
      '--frame_analyzer=%s' % path_to_analyzer,
      '--yuv_frame_width=%d' % width,
      '--yuv_frame_height=%d' % height,
      '--stats_file=%s' % stats_filename,
    ]
    print 'Start command: ', ' '.join(cmd)

    compare_videos = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                                      stderr=subprocess.PIPE)
    output, error = compare_videos.communicate()
    if compare_videos.returncode != 0:
      raise FailedToRunToolException('Failed to run compare videos script!')

    return output

  def _ProcessFramesCountOutput(self, output):
    """Processes the analyzer output for the different frame counts.

    The frame analyzer outputs additional information about the number of unique
    frames captured, The max number of repeated frames in a sequence and the
    max number of skipped frames. These values are then written to the Perf
    Graph. (Note: Some of the repeated or skipped frames will probably be due to
    the imperfection of JavaScript timers.)

    Args:
      output(string): The output from the frame analyzer to be processed.
    """
    # The output from frame analyzer will be in the format:
    # <PSNR and SSIM stats>
    # Unique_frames_count:<value>
    # Max_repeated:<value>
    # Max_skipped:<value>
    unique_fr_pos = output.rfind('Unique_frames_count')
    result_str = output[unique_fr_pos:]

    result_list = result_str.split()

    for result in result_list:
      colon_pos = result.find(':')
      key = result[:colon_pos]
      value = result[colon_pos+1:]
      pyauto_utils.PrintPerfResult(key, 'VGA', value, '')

  def _ProcessPsnrAndSsimOutput(self, output):
    """Processes the analyzer output to extract the PSNR and SSIM values.

    The frame analyzer produces PSNR and SSIM results for every unique frame
    that has been captured. This method forms a list of all the psnr and ssim
    values and passes it to PrintPerfResult() for printing on the Perf Graph.

    Args:
      output(string): The output from the frame analyzer to be processed.
    """
    # The output is in the format:
    # BSTATS
    # psnr ssim; psnr ssim; ... psnr ssim;
    # ESTATS
    stats_beginning = output.find('BSTATS')  # Get the beginning of the stats
    stats_ending = output.find('ESTATS')  # Get the end of the stats
    stats_str = output[(stats_beginning + len('BSTATS')):stats_ending]

    stats_list = stats_str.split(';')

    psnr = []
    ssim = []

    for item in stats_list:
      item = item.strip()
      if item != '':
        entry = item.split(' ')
        psnr.append(float(entry[0]))
        ssim.append(float(entry[1]))

    pyauto_utils.PrintPerfResult('PSNR', 'VGA', psnr, '')
    pyauto_utils.PrintPerfResult('SSIM', 'VGA', ssim, '')


if __name__ == '__main__':
  pyauto_functional.Main()
