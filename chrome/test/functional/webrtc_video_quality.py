#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys

import pyauto_functional
import pyauto
import pyauto_utils
import webrtc_test_base

# If you change the port number, don't forget to modify video_extraction.js too.
_PYWEBSOCKET_PORT_NUMBER = '12221'


class MissingRequiredToolException(Exception):
  pass


class WebrtcVideoQualityTest(webrtc_test_base.WebrtcTestBase):
  """Test the video quality of the WebRTC output.

  Prerequisites: This test case must run on a machine with a webcam, either
  fake or real, and with some kind of audio device. You must make the
  peerconnection_server target before you run.

  The test case will launch a custom binary (peerconnection_server) which will
  allow two WebRTC clients to find each other.

  The test also runs several other custom binaries - rgba_to_i420 converter and
  frame_analyzer. Both tools can be found under third/party/webrtc/tools. The
  test also runs a standalone Python implementation of a WebSocket server
  (pywebsocket) and a barcode_decoder script.
  """

  def setUp(self):
    pyauto.PyUITest.setUp(self)
    self.StartPeerConnectionServer()

  def tearDown(self):
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

    self.EstablishCall(from_tab_with_index=0)

    # Wait for JavaScript to capture all the frames. In the HTML file we specify
    # how many seconds to capture frames. The default for retry_sleep is 0.25
    # but there is no need to ask this often whether we are done capturing the
    # frames. It seems that 1 second is more reasonable retry time.
    done_capturing = self.WaitUntil(
        function=lambda: self.ExecuteJavascript('doneFrameCapturing()',
                                                tab_index=1),
        expect_retval='done-capturing', retry_sleep=1.0)

    self.assertTrue(done_capturing,
                    msg='Timed out while waiting frames to be captured.')

    # The hang-up will automatically propagate to the second tab.
    self.HangUp(from_tab_with_index=0)
    self.VerifyHungUp(tab_index=1)

    self.Disconnect(tab_index=0)
    self.Disconnect(tab_index=1)

    # Ensure we didn't miss any errors.
    self.AssertNoFailures(tab_index=0)
    self.AssertNoFailures(tab_index=1)

  def testVgaVideoQuality(self):
    """Tests the WebRTC video output for a VGA video input.

    On the bots we will be running fake webcam driver and we will feed a video
    with overlaid barcodes. In order to run the analysis on the output, we need
    to use the original input video as a reference video. We take the name of
    this file from an environment variable that the bots set.
    """
    ref_file = os.environ['PYAUTO_REFERENCE_FILE']
    self._StartVideoQualityTest(test_page='webrtc_video_quality_test.html',
                                helper_page='webrtc_jsep_test.html',
                                reference_yuv=ref_file, width=640,
                                height=480, barcode_height=32)


  def _StartVideoQualityTest(self, reference_yuv,
                             test_page='webrtc_video_quality_test.html',
                             helper_page='webrtc_jsep_test.html',
                             width=640, height=480, barcode_height=32):
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
      barcode_height(int): The height of the barcodes overlaid in every frame of
        the video.
    """
    self._StartPywebsocketServer()

    self._WebRtcCallWithHelperPage(test_page, helper_page)

    # Wait for JavaScript to send all the frames to the server. The test will
    # have quite a lot of frames to send, so it will take at least several
    # seconds. Thus there is no need to ask whether there are more frames to
    # send every fourth of a second.
    no_more_frames = self.WaitUntil(
        function=lambda: self.ExecuteJavascript('haveMoreFramesToSend()',
                                                tab_index=1),
        expect_retval='no-more-frames', retry_sleep=0.5, timeout=150)
    self.assertTrue(no_more_frames,
                    msg='Timed out while waiting for frames to send.')

    self._StopPywebsocketServer()

    self.assertTrue(self._RunRGBAToI420Converter(width, height))
    self.assertTrue(self._RunBarcodeDecoder(width, height, barcode_height))

    analysis_result = self._RunFrameAnalyzer(width, height, reference_yuv)
    self._ProcessPsnrAndSsimOutput(analysis_result)
    self._ProcessFramesCountOutput(analysis_result)

  def _StartPywebsocketServer(self):
    """Starts the pywebsocket server."""
    print 'Starting pywebsocket server.'
    path_to_base = os.path.join(self.BrowserPath(), '..', '..')

    # Pywebsocket source directory.
    path_pyws_dir = os.path.join(path_to_base, 'third_party', 'WebKit',
                                 'Tools', 'Scripts', 'webkitpy', 'thirdparty')

    # Pywebsocket standalone server.
    path_to_pywebsocket= os.path.join(path_pyws_dir, 'mod_pywebsocket',
                                      'standalone.py')

    # Path to the data handler to handle data received by the server.
    path_to_handler = os.path.join(path_to_base, 'chrome', 'test', 'functional')

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
    handler_output_path = os.path.join(path_to_base, '..', '..')
    env['PYWS_DIR_FOR_HANDLER_OUTPUT'] = os.path.abspath(handler_output_path)

    # Start the pywebsocket server. The server will not start instantly, so the
    # code opening websockets to it should take this into account.
    self._pywebsocket_server = subprocess.Popen(start_cmd, env=env)

  def _StopPywebsocketServer(self):
    """Stops the running instance of pywebsocket server."""
    print 'Stopping pywebsocket server.'
    assert self._pywebsocket_server
    self._pywebsocket_server.kill()

  def _RunRGBAToI420Converter(self, width, height):
    """Runs the RGBA to I420 converter.

    The rgba_to_i420_converter is part of the webrtc_pyauto_tools target which
    should be build prior to running this test. The resulting binary should live
    next to Chrome.

    Args:
      width(int): The width of the frames to be converted and stitched together.
      height(int): The height of the frames to be converted and stitched.
    Return:
      (bool): True if the conversion is successful, false otherwise.
    """
    path_to_rgba_converter = os.path.join(self.BrowserPath(),
                                          'rgba_to_i420_converter')
    path_to_rgba_converter = os.path.abspath(path_to_rgba_converter)
    path_to_rgba_converter = self.BinPathForPlatform(path_to_rgba_converter)

    if not os.path.exists(path_to_rgba_converter):
      raise webrtc_test_base.MissingRequiredBinaryException(
          'Could not locate rgba_to_i420_converter! Did you build the '
          'webrtc_pyauto_tools target?')

    # This is where the pywebsocket handler writes the captured frames.
    frames_dir = os.environ['PYWS_DIR_FOR_HANDLER_OUTPUT']

    # We produce an output file that will later be used as an input to the
    # barcode decoder and frame analyzer tools.
    output_file = os.path.join(frames_dir, 'pyauto_output.yuv')
    start_cmd = [path_to_rgba_converter, '--frames_dir=%s' % frames_dir,
                 '--output_file=%s' % output_file, '--width=%d' % width,
                 '--height=%d' % height, '--delete_frames']
    print 'Start command: ', ' '.join(start_cmd)
    rgba_converter = subprocess.Popen(start_cmd, stdout=subprocess.PIPE)
    output = rgba_converter.communicate()[0]
    if 'Unsuccessful' in output:
      print 'Unsuccessful RGBA to I420 conversion'
      return False
    return True

  def _RunBarcodeDecoder(self, width, height, barcode_height):
    """Runs the barcode decoder.

    The barcode decoder decodes the barcode overlaid into every frame of the
    YUV file produced by rgba_to_i420_converter. It writes the relation between
    the frame number in this output file and the frame number (the decoded
    barcode value) in the original input file.

    The barcode decoder uses a big java library for decoding the barcodes called
    Zxing (Zebra crossing). It is checked in the webrtc-tools repo which isn't
    synced in Chrome. That's why we check it out next to Chrome using a
    modified version of the .gclient file and than build the necessary jars for
    the library to be usable.

    Args:
      width(int): The frames width of the video to be decoded.
      height(int): The frames height of the video to be decoded.
      barcode_height(int): The height of the barcodes overlaid on top of every
        frame.
    Return:
      (bool): True if the decoding was successful, False otherwise.
    """
    # The barcode decoder lives in folder barcode_tools next to src.
    path_to_decoder = os.path.join(self.BrowserPath(), '..', '..', '..',
                                  'barcode_tools', 'barcode_decoder.py')
    path_to_decoder = os.path.abspath(path_to_decoder)

    if not os.path.exists(path_to_decoder):
      raise MissingRequiredToolException(
          'Could not locate the barcode decoder tool! The barcode decoder '
          'decodes the barcodes overlaid on top of every frame of the captured '
          'video. As it uses a big Java library, it is checked in the '
          'webrtc-tools repo which isn\'t synced in Chrome. Check it out next '
          'to Chrome\' src by modifying the .gclient file. Than build the '
          'necessary jar files for the library to become usable. You can build '
          'them by running the build_zxing.py script.')
    # The YUV file is the file produced by rgba_to_i420_converter.
    yuv_file = os.path.join(os.environ['PYWS_DIR_FOR_HANDLER_OUTPUT'],
                            'pyauto_output.yuv')
    stats_file = os.path.join(os.environ['PYWS_DIR_FOR_HANDLER_OUTPUT'],
                            'pyauto_stats.txt')
    python_interp = sys.executable
    start_cmd = [python_interp, path_to_decoder, '--yuv_file=%s' % yuv_file,
                 '--yuv_frame_width=%d' % width,
                 '--yuv_frame_height=%d' % height,
                 '--barcode_height=%d' % barcode_height,
                 '--stats_file=%s' % stats_file]
    print 'Start command: ', ' '.join(start_cmd)

    barcode_decoder = subprocess.Popen(start_cmd, stderr=subprocess.PIPE)
    error = barcode_decoder.communicate()[1]
    if error:
      print 'Error: ', error
      return False
    return True

  def _RunFrameAnalyzer(self, width, height, reference_yuv):
    """Runs the frame analyzer tool for PSNR and SSIM analysis.

    The frame analyzer is also part of the webrtc_pyauto_target. It should be
    built before running this test. We assume that the binary will end up next
    to Chrome.

    Frame analyzer prints its output to the standard output from where it has to
    be read and processed.

    Args:
      width(int): The width of the video frames to be analyzed.
      height(int): The height of the video frames to be analyzed.
      reference_yuv(string): The name of the reference video to be used as a
        reference during the analysis.
    Return:
      (string): The output from the frame_analyzer.
    """
    path_to_analyzer = os.path.join(self.BrowserPath(), 'frame_analyzer')
    path_to_analyzer = os.path.abspath(path_to_analyzer)

    path_to_analyzer = self.BinPathForPlatform(path_to_analyzer)

    if not os.path.exists(path_to_analyzer):
      raise webrtc_test_base.MissingRequiredBinaryException(
          'Could not locate frame_analyzer! Did you build the '
          'webrtc_pyauto_tools target?')

    # We assume that the reference file(s) will be in the same directory where
    # the captured frames, the output and stats files are written.
    ref_file = os.path.join(os.environ['PYWS_DIR_FOR_HANDLER_OUTPUT'],
                            reference_yuv)
    test_file = os.path.join(os.environ['PYWS_DIR_FOR_HANDLER_OUTPUT'],
                            'pyauto_output.yuv')
    stats_file = os.path.join(os.environ['PYWS_DIR_FOR_HANDLER_OUTPUT'],
                            'pyauto_stats.txt')
    start_cmd = [path_to_analyzer, '--reference_file=%s' % ref_file,
                 '--test_file=%s' % test_file, '--stats_file=%s' % stats_file,
                 '--width=%d' % width, '--height=%d' % height]
    print 'Start command: ', ' '.join(start_cmd)

    frame_analyzer = subprocess.Popen(start_cmd, stdout=subprocess.PIPE,
                                      stderr=subprocess.PIPE)
    output, error = frame_analyzer.communicate()
    if error:
      print 'Error: ', error
      return 'BSTATS undef undef; ESTATS'
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
