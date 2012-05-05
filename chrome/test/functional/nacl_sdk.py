#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import ctypes
from distutils import version
import fnmatch
import glob
import hashlib
import logging
import os
import platform
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib2
import xml.dom.minidom

import pyauto_functional  # Must be imported before pyauto.
import pyauto
import pyauto_utils


class NaClSDKTest(pyauto.PyUITest):
  """Tests for the NaCl SDK."""
  _extracted_sdk_path = None
  _temp_dir = None
  _settings = {
      'release_win_sdk_url': 'http://commondatastorage.googleapis.com/'
      'nativeclient-mirror/nacl/nacl_sdk/staging/naclsdk_win.exe',
      'release_mac_sdk_url': 'http://commondatastorage.googleapis.com/'
      'nativeclient-mirror/nacl/nacl_sdk/staging/naclsdk_mac.tgz',
      'release_lin_sdk_url': 'http://commondatastorage.googleapis.com/'
      'nativeclient-mirror/nacl/nacl_sdk/staging/naclsdk_linux.tgz',
      'expected_md5_url': 'http://commondatastorage.googleapis.com/'
      'nativeclient-mirror',
      'release_win_expected_md5_key': 'nacl/nacl_sdk/staging/naclsdk_win.exe',
      'release_mac_expected_md5_key': 'nacl/nacl_sdk/staging/naclsdk_mac.tgz',
      'release_lin_expected_md5_key': 'nacl/nacl_sdk/staging/'
      'naclsdk_linux.tgz',
      'post_sdk_download_url': 'http://code.google.com/chrome/nativeclient/'
      'docs/download.html',
      'post_win_sdk_url': 'http://commondatastorage.googleapis.com/'
      'nativeclient-mirror/nacl/nacl_sdk/naclsdk_win.exe',
      'post_mac_sdk_url': 'http://commondatastorage.googleapis.com/'
      'nativeclient-mirror/nacl/nacl_sdk/naclsdk_mac.tgz',
      'post_lin_sdk_url': 'http://commondatastorage.googleapis.com/'
      'nativeclient-mirror/nacl/nacl_sdk/naclsdk_linux.tgz',
      'min_required_chrome_build': 14,
      'gallery_examples': {
           'life': 'http://nacl-gallery.appspot.com/life/life.html',
           'hello_world': 'http://nacl-gallery.appspot.com/hello_world/'
           'hello_world.html',
           'pi_generator': 'http://nacl-gallery.appspot.com/pi_generator/'
           'pi_generator.html',
           'sine_synth': 'http://nacl-gallery.appspot.com/sine_synth/'
           'sine_synth.html'
      },
      'prerelease_gallery': {
           'hello_world': 'http://4.nacl-gallery.appspot.com/hello_world/'
           'hello_world.html',
           'hello_world_c': 'http://4.nacl-gallery.appspot.com/hello_world_c/'
           'hello_world.html',
           'life': 'http://4.nacl-gallery.appspot.com/life/life.html',
           'pi_generator': 'http://4.nacl-gallery.appspot.com/pi_generator/'
           'pi_generator.html',
           'sine_synth': 'http://4.nacl-gallery.appspot.com/sine_synth/'
           'sine_synth.html',
           'geturl': 'http://4.nacl-gallery.appspot.com/geturl/geturl.html'
      }
  }

  def tearDown(self):
    pyauto.PyUITest.tearDown(self)
    self._RemoveDownloadedTestFile()

  def testNaClSDK(self):
    """Verify that NaCl SDK is working properly."""
    if not self._HasAllSystemRequirements():
      logging.info('System does not meet the requirements.')
      return
    self._extracted_sdk_path = tempfile.mkdtemp()

    self._VerifyDownloadLinks()
    self._VerifyNaClSDKInstaller()
    self._VerifyBuildStubProject()
    self._LaunchServerAndVerifyExamples()
    self._VerifyRebuildExamples()
    self._VerifySelldrAndNcval()

  def testVerifyNaClSDKChecksum(self):
    """Verify NaCl SDK Checksum."""
    if not self._HasAllSystemRequirements():
      logging.info('System does not meet the requirements.')
      return

    self._DownloadNaClSDK()

    if pyauto.PyUITest.IsWin():
      expected_md5_key = self._settings['release_win_expected_md5_key']
      file_path = os.path.join(self._temp_dir, 'naclsdk_win.exe')
    elif pyauto.PyUITest.IsMac():
      expected_md5_key = self._settings['release_mac_expected_md5_key']
      file_path = os.path.join(self._temp_dir, 'naclsdk_mac.tgz')
    elif pyauto.PyUITest.IsLinux():
      expected_md5_key = self._settings['release_lin_expected_md5_key']
      file_path = os.path.join(self._temp_dir, 'naclsdk_linux.tgz')
    else:
      self.fail(msg='NaCl SDK does not support this OS.')

    # Get expected MD5.
    expected_md5_url = self._settings['expected_md5_url']
    response = urllib2.urlopen(expected_md5_url)
    dom = xml.dom.minidom.parseString(response.read())
    dom_content = dom.getElementsByTagName('Contents')
    expected_md5 = None
    for con in dom_content:
      if (self._GetXMLNodeData(con.getElementsByTagName('Key')[0].childNodes)
          == expected_md5_key):
        node = con.getElementsByTagName('ETag')[0].childNodes
        expected_md5 = self._GetXMLNodeData(node).strip('"')
    self.assertTrue(expected_md5,
                    msg='Cannot get expected MD5 from %s.' % expected_md5_url)

    md5 = hashlib.md5()
    md5.update(open(file_path).read())
    md5_sum = md5.hexdigest()
    self.assertEqual(expected_md5, md5_sum,
                     msg='Unexpected checksum. Expected: %s, got: %s'
                     % (expected_md5, md5_sum))

  def testVerifyNaClPlugin(self):
    """Verify NaCl plugin."""
    if not self._HasAllSystemRequirements():
      logging.info('System does not meet the requirements.')
      return
    self._OpenExamplesAndStartTest(
        self._settings['gallery_examples'])

  def testVerifyPrereleaseGallery(self):
    """Verify Pre-release gallery examples."""
    if not self._HasAllSystemRequirements():
      logging.info('System does not meet the requirements.')
      return
    self._OpenExamplesAndStartTest(
        self._settings['prerelease_gallery'])

  def _VerifyDownloadLinks(self):
    """Verify the download links."""
    html = None
    for i in xrange(3):
      try:
        html = urllib2.urlopen(self._settings['post_sdk_download_url']).read()
        break
      except:
        pass
    self.assertTrue(html,
                    msg='Cannot open URL: %s' %
                    self._settings['post_sdk_download_url'])

    # Make sure the correct URL is under the correct label.
    if pyauto.PyUITest.IsWin():
      win_sdk_url = self._settings['post_win_sdk_url']
      win_url_index = html.find(win_sdk_url)
      self.assertTrue(win_url_index > -1,
                      msg='Missing SDK download URL: %s' % win_sdk_url)
      win_keyword_index = html.rfind('Windows', 0, win_url_index)
      self.assertTrue(win_keyword_index > -1,
                      msg='Misplaced download link: %s' % win_sdk_url)
    elif pyauto.PyUITest.IsMac():
      mac_sdk_url = self._settings['post_mac_sdk_url']
      mac_url_index = html.find(mac_sdk_url)
      self.assertTrue(mac_url_index > -1,
                      msg='Missing SDK download URL: %s' % mac_sdk_url)
      mac_keyword_index = html.rfind('Macintosh', 0, mac_url_index)
      self.assertTrue(mac_keyword_index > -1,
                      msg='Misplaced download link: %s' % mac_sdk_url)
    elif pyauto.PyUITest.IsLinux():
      lin_sdk_url = self._settings['post_lin_sdk_url']
      lin_url_index = html.find(lin_sdk_url)
      self.assertTrue(lin_url_index > -1,
                      msg='Missing SDK download URL: %s' % lin_sdk_url)
      lin_keyword_index = html.rfind('Linux', 0, lin_url_index)
      self.assertTrue(lin_keyword_index > -1,
                      msg='Misplaced download link: %s' % lin_sdk_url)
    else:
      self.fail(msg='NaCl SDK does not support this OS.')

  def _VerifyNaClSDKInstaller(self):
    """Verify NaCl SDK installer."""
    search_list = [
        'build.scons',
        'favicon.ico',
        'geturl/',
        'hello_world/',
        'hello_world_c/',
        'httpd.py',
        'index.html',
        'nacl_sdk_scons/',
        'pi_generator/',
        'scons',
        'sine_synth/'
    ]

    mac_lin_additional_search_items = [
        'sel_ldr_x86_32',
        'sel_ldr_x86_64',
        'ncval_x86_32',
        'ncval_x86_64'
    ]

    win_additional_search_items = [
        'httpd.cmd',
        'sel_ldr_x86_32.exe',
        'sel_ldr_x86_64.exe',
        'ncval_x86_32.exe',
        'ncval_x86_64.exe'
    ]

    self._DownloadNaClSDK()

    if pyauto.PyUITest.IsWin():
      self._ExtractNaClSDK()
      self._SearchNaClSDKFileWindows(
          search_list + win_additional_search_items)
    elif pyauto.PyUITest.IsMac():
      source_file = os.path.join(self._temp_dir, 'naclsdk_mac.tgz')
      self._SearchNaClSDKTarFile(search_list + mac_lin_additional_search_items,
                                 source_file)
      self._ExtractNaClSDK()
    elif pyauto.PyUITest.IsLinux():
      source_file = os.path.join(self._temp_dir, 'naclsdk_linux.tgz')
      self._SearchNaClSDKTarFile(search_list + mac_lin_additional_search_items,
                                 source_file)
      self._ExtractNaClSDK()
    else:
      self.fail(msg='NaCl SDK does not support this OS.')

  def _VerifyBuildStubProject(self):
    """Build stub project."""
    stub_project_files = [
        'build.scons',
        'scons'
    ]
    project_template_path = self._GetDirectoryPath('project_templates',
                                                   self._extracted_sdk_path)
    examples_path = self._GetDirectoryPath('examples',
                                           self._extracted_sdk_path)
    init_project_path = os.path.join(project_template_path, 'init_project.py')

    # Build a C project.
    subprocess.call(
        ['python', init_project_path, '-n', 'hello_c', '-c', '-d',
        examples_path], stdout=subprocess.PIPE)

    hello_c_path = os.path.join(examples_path, 'hello_c')
    for file in stub_project_files:
        self.assertTrue(self._HasFile(file, hello_c_path),
                        msg='Cannot build C stub project.')

    # Build a C++ project.
    subprocess.call(
        ['python', init_project_path, '-n', 'hello_cc', '-d',
        examples_path], stdout=subprocess.PIPE)

    hello_cc_path = os.path.join(examples_path, 'hello_cc')
    for file in stub_project_files:
        self.assertTrue(self._HasFile(file, hello_cc_path),
                        msg='Cannot build C++ stub project.')

  def _LaunchServerAndVerifyExamples(self):
    """Start local HTTP server and verify examples."""
    # Make sure server is not open.
    if self._IsURLAlive('http://localhost:5103'):
      self._CloseHTTPServer()

    # Start HTTP server.
    examples_path = self._GetDirectoryPath('examples',
                                           self._extracted_sdk_path)
    if pyauto.PyUITest.IsWin():
      http_path = os.path.join(examples_path, 'httpd.cmd')
      proc = subprocess.Popen([http_path], cwd=examples_path)
    else:
      http_path = os.path.join(examples_path, 'httpd.py')
      proc = subprocess.Popen(['python', http_path], cwd=examples_path)

    success = self.WaitUntil(
        lambda: self._IsURLAlive('http://localhost:5103'),
        timeout=30, retry_sleep=1, expect_retval=True)
    self.assertTrue(success,
                    msg='Cannot open HTTP server. %s' %
                    self.GetActiveTabTitle())

    examples = {
        'hello_world_c': 'http://localhost:5103/hello_world_c/'
        'hello_world.html',
        'hello_world': 'http://localhost:5103/hello_world/hello_world.html',
        'geturl': 'http://localhost:5103/geturl/geturl.html',
        'pi_generator': 'http://localhost:5103/pi_generator/pi_generator.html',
        'sine_synth': 'http://localhost:5103/sine_synth/sine_synth.html',
    }
    try:
      self._OpenExamplesAndStartTest(examples)
    finally:
      self._CloseHTTPServer(proc)

  def _VerifyRebuildExamples(self):
    """Re-build the examples and verify they are as expected."""
    example_dirs = [
        'geturl',
        'hello_world',
        'hello_world_c',
        'pi_generator',
        'sine_synth'
    ]
    examples_path = self._GetDirectoryPath('examples',
                                           self._extracted_sdk_path)

    scons_path = os.path.join(examples_path, 'scons -c')
    subprocess.call([scons_path], cwd=examples_path, shell=True)
    for x in example_dirs:
      ex_path = os.path.join(examples_path, x)
      self.assertFalse(self._HasFile('*.nmf', ex_path),
                       msg='Failed running scons -c.')

    scons_path = os.path.join(examples_path, 'scons')
    proc = subprocess.Popen([scons_path], cwd=examples_path,
                            stdout=subprocess.PIPE, shell=True)
    proc.communicate()

    # Verify each example directory contains .nmf file.
    for dir in example_dirs:
      dir = os.path.join(examples_path, dir)
      if not self._HasFile('*.nmf', dir):
        self.fail(msg='Failed running scons.')

    self._LaunchServerAndVerifyExamples()

  def _VerifySelldrAndNcval(self):
    """Verify sel_ldr and ncval."""
    architecture = self._GetPlatformArchitecture()
    scons_arg = None
    if pyauto.PyUITest.IsWin():
      if architecture == '64bit':
        scons_arg = 'test64'
      else:
        scons_arg = 'test32'
    elif pyauto.PyUITest.IsMac():
      scons_arg = 'test64'
    elif pyauto.PyUITest.IsLinux():
      scons_arg = 'test64'

    examples_path = self._GetDirectoryPath('examples',
                                           self._extracted_sdk_path)
    scons_path = os.path.join(examples_path, 'scons ' + scons_arg)

    # Build and run the unit test.
    proc = subprocess.Popen([scons_path], stdout=subprocess.PIPE,
                            shell=True, cwd=examples_path)
    lines = proc.communicate()[0].splitlines()
    test_ran = False

    for line in lines:
      if 'Check:' in line:
        self.assertTrue('passed' in line,
                        msg='Nacl-sel_ldr unit test failed.')
        test_ran = True
    self.assertTrue(test_ran,
                    msg='Failed to build and run nacl-sel_ldr unit test.')

    if architecture == '64bit':
      self.assertTrue(
          self._HasPathInTree('hello_world_x86_64.nexe',
                              True, root=examples_path),
          msg='Missing file: hello_world_x86_64.nexe.')
    else:
      self.assertTrue(
          self._HasPathInTree('hello_world_x86_32.nexe',
                              True, root=examples_path),
          msg='Missing file: hello_world_x86_32.nexe.')

    # Verify that a mismatch of sel_ldr and .nexe produces an error.
    toolchain_path = self._GetDirectoryPath('toolchain',
                                            self._extracted_sdk_path)
    bin_path = self._GetDirectoryPath('bin', toolchain_path)
    hello_world_path = self._GetDirectoryPath('hello_world', examples_path)
    sel_32_path = os.path.join(bin_path, 'sel_ldr_x86_32')
    sel_64_path = os.path.join(bin_path, 'sel_ldr_x86_64')
    nexe_32_path = os.path.join(hello_world_path, 'hello_world_x86_32.nexe')
    nexe_64_path = os.path.join(hello_world_path, 'hello_world_x86_64.nexe')

    if architecture == '64bit':
      success = self._RunProcessAndCheckOutput(
          [sel_64_path, nexe_32_path], 'Error while loading')
    else:
      success = self._RunProcessAndCheckOutput(
          [sel_32_path, nexe_64_path], 'Error while loading')
    self.assertTrue(success,
                    msg='Failed to verify sel_ldr and .nexe mismatch.')

    # Run the appropriate ncval for the platform on the matching .nexe.
    ncval_32_path = os.path.join(bin_path, 'ncval_x86_32')
    ncval_64_path = os.path.join(bin_path, 'ncval_x86_64')

    if architecture == '64bit':
      success = self._RunProcessAndCheckOutput(
          [ncval_64_path, nexe_64_path], 'is safe')
    else:
      success = self._RunProcessAndCheckOutput(
          [ncval_32_path, nexe_32_path], 'is safe')
    self.assertTrue(success, msg='Failed to verify ncval.')

    # Verify that a mismatch of ncval and .nexe produces an error.
    if architecture == '64bit':
      success = self._RunProcessAndCheckOutput(
          [ncval_64_path, nexe_32_path], 'is safe', is_in=False)
    else:
      success = self._RunProcessAndCheckOutput(
          [ncval_32_path, nexe_64_path], 'is safe', is_in=False)
    self.assertTrue(success, msg='Failed to verify ncval and .nexe mismatch.')

  def _RemoveDownloadedTestFile(self):
    """Delete downloaded files and dirs from downloads directory."""
    if self._extracted_sdk_path and os.path.exists(self._extracted_sdk_path):
      self._CloseHTTPServer()

      def _RemoveFile():
        shutil.rmtree(self._extracted_sdk_path, ignore_errors=True)
        return os.path.exists(self._extracted_sdk_path)

      success = self.WaitUntil(_RemoveFile, retry_sleep=2, expect_retval=False)
      self.assertTrue(success,
                      msg='Cannot remove %s' % self._extracted_sdk_path)

    if self._temp_dir:
      pyauto_utils.RemovePath(self._temp_dir)

  def _RunProcessAndCheckOutput(self, args, look_for, is_in=True):
    """Run process and look for string in output.

    Args:
      args: Argument strings to pass to subprocess.
      look_for: The string to search in output.
      is_in: True if checking if param look_for is in output.
             False if checking if param look_for is not in output.

    Returns:
      True, if output contains parameter |look_for| and |is_in| is True, or
      False otherwise.
    """
    proc = subprocess.Popen(args, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
    (stdout, stderr) = proc.communicate()
    lines = (stdout + stderr).splitlines()
    for line in lines:
      if look_for in line:
        return is_in
    return not is_in

  def _OpenExamplesAndStartTest(self, examples):
    """Open each example and verify that it's working.

    Args:
      examples: A dict of name to url of examples.
    """
    self._EnableNaClPlugin()

    # Open all examples.
    for name, url in examples.items():
      self.AppendTab(pyauto.GURL(url))
      self._CheckForCrashes()

    # Verify all examples are working.
    for name, url in examples.items():
      self._VerifyAnExample(name, url)
    self._CheckForCrashes()

    # Reload all examples.
    for _ in range(2):
      for tab_index in range(self.GetTabCount()):
        self.GetBrowserWindow(0).GetTab(tab_index).Reload()
    self._CheckForCrashes()

    # Verify all examples are working.
    for name, url in examples.items():
      self._VerifyAnExample(name, url)
    self._CheckForCrashes()

    # Close each tab, check for crashes and verify all open
    # examples operate correctly.
    tab_count = self.GetTabCount()
    for index in xrange(tab_count - 1, 0, -1):
      self.GetBrowserWindow(0).GetTab(index).Close(True)
      self._CheckForCrashes()

      tabs = self.GetBrowserInfo()['windows'][0]['tabs']
      for tab in tabs:
        if tab['index'] > 0:
          for name, url in examples.items():
            if url == tab['url']:
              self._VerifyAnExample(name, url)
              break

  def _VerifyAnExample(self, name, url):
    """Verify NaCl example is working.

    Args:
      name: A string name of the example.
      url: A string url of the example.
    """
    available_example_tests = {
      'hello_world_c': self._VerifyHelloWorldExample,
      'hello_world': self._VerifyHelloWorldExample,
      'pi_generator': self._VerifyPiGeneratorExample,
      'sine_synth': self._VerifySineSynthExample,
      'geturl': self._VerifyGetURLExample,
      'life': self._VerifyConwaysLifeExample
    }

    if not name in available_example_tests:
      self.fail(msg='No test available for %s.' % name)

    info = self.GetBrowserInfo()
    tabs = info['windows'][0]['tabs']
    tab_index = None
    for tab in tabs:
      if url == tab['url']:
        self.ActivateTab(tab['index'])
        tab_index = tab['index']
        break

    if tab_index:
      available_example_tests[name](tab_index, name, url)

  def _VerifyHelloWorldExample(self, tab_index, name, url):
    """Verify Hello World Example.

    Args:
      tab_index: Tab index integer that the example is on.
      name: A string name of the example.
      url: A string url of the example.
    """
    success = self.WaitUntil(
        lambda: self.GetDOMValue(
                    'document.getElementById("statusField").innerHTML',
                    tab_index),
        timeout=60, expect_retval='SUCCESS')
    self.assertTrue(success, msg='Example %s failed. URL: %s' % (name, url))

    js_code = """
      window.alert = function(e) {
        window.domAutomationController.send(String(e));
      }
      window.domAutomationController.send("done");
    """
    self.ExecuteJavascript(js_code, tab_index)

    result = self.ExecuteJavascript('document.helloForm.elements[1].click();',
                                    tab_index)
    self.assertEqual(result, '42',
                     msg='Example %s failed. URL: %s' % (name, url))

    result = self.ExecuteJavascript('document.helloForm.elements[2].click();',
                                    tab_index)
    self.assertEqual(result, 'dlrow olleH',
                     msg='Example %s failed. URL: %s' % (name, url))

  def _VerifyPiGeneratorExample(self, tab_index, name, url):
    """Verify Pi Generator Example.

    Args:
      tab_index: Tab index integer that the example is on.
      name: A string name of the example.
      url: A string url of the example.
    """
    success = self.WaitUntil(
        lambda: self.GetDOMValue('document.form.pi.value', 0, tab_index)[0:3],
        timeout=120, expect_retval='3.1')
    self.assertTrue(success, msg='Example %s failed. URL: %s' % (name, url))

    # Get top corner of Pi image.
    js_code = """
      var obj = document.getElementById('piGenerator');
      var curleft = curtop = 0;
      do {
          curleft += obj.offsetLeft;
          curtop += obj.offsetTop;
      } while (obj = obj.offsetParent);
      window.domAutomationController.send(curleft + ", " + curtop);
    """
    result = self.ExecuteJavascript(js_code, tab_index)
    result_split = result.split(', ')
    x = int(result_split[0])
    y = int(result_split[1])
    window = self.GetBrowserInfo()['windows'][0]
    window_to_content_x = 2
    window_to_content_y = 80
    pi_image_x = x + window['x'] + window_to_content_x
    pi_image_y = y + window['y'] + window_to_content_y

    if self._IsGetPixelSupported():
      is_animating = self._IsColorChanging(pi_image_x, pi_image_y, 50, 50)
      self.assertTrue(is_animating,
                      msg='Example %s failed. URL: %s' % (name, url))

  def _VerifySineSynthExample(self, tab_index, name, url):
    """Verify Sine Wave Synthesizer Example.

    Args:
      tab_index: Tab index integer that the example is on.
      name: A string name of the example.
      url: A string url of the example.
    """
    success = self.WaitUntil(
        lambda: self.GetDOMValue(
                    'document.getElementById("frequency_field").value',
                    tab_index),
        timeout=30, expect_retval='440')
    self.assertTrue(success, msg='Example %s failed. URL: %s' % (name, url))

    self.ExecuteJavascript(
        'document.body.getElementsByTagName("button")[0].click();'
        'window.domAutomationController.send("done")',
        tab_index)

    # TODO(chrisphan): Verify sound.

  def _VerifyGetURLExample(self, tab_index, name, url):
    """Verify GetURL Example.

    Args:
      tab_index: Tab index integer that the example is on.
      name: A string name of the example.
      url: A string url of the example.
    """
    success = self.WaitUntil(
        lambda: self.GetDOMValue(
                    'document.getElementById("status_field").innerHTML',
                    tab_index),
        timeout=60, expect_retval='SUCCESS')
    self.assertTrue(success, msg='Example %s failed. URL: %s' % (name, url))

    self.ExecuteJavascript(
        'document.geturl_form.elements[0].click();'
        'window.domAutomationController.send("done")',
        tab_index)

    js_code = """
      var output = document.getElementById("general_output").innerHTML;
      var result;
      if (output.indexOf("test passed") != -1)
        result = "pass";
      else
        result = "fail";
      window.domAutomationController.send(result);
    """
    success = self.WaitUntil(
        lambda: self.ExecuteJavascript(js_code, tab_index),
        timeout=30, expect_retval='pass')
    self.assertTrue(success, msg='Example %s failed. URL: %s' % (name, url))

  def _VerifyConwaysLifeExample(self, tab_index, name, url):
    """Verify Conway's Life Example.

    Args:
      tab_index: Tab index integer that the example is on.
      name: A string name of the example.
      url: A string url of the example.
    """
    window = self.GetBrowserInfo()['windows'][0]
    window_to_content_x = 2
    window_to_content_y = 80
    x = window['x'] + window_to_content_x
    y = window['y'] + window_to_content_y
    offset_pixel = 100
    if self._IsGetPixelSupported():
      success = self.WaitUntil(
          lambda: self._GetPixel(x + offset_pixel, y + offset_pixel),
          timeout=30, expect_retval=16777215)
      self.assertTrue(success, msg='Example %s failed. URL: %s' % (name, url))

  def _GetXMLNodeData(self, nodelist):
      rc = []
      for node in nodelist:
          if node.nodeType == node.TEXT_NODE:
              rc.append(node.data)
      return ''.join(rc)

  def _IsColorChanging(self, x, y, width, height):
    """Check screen for anything that is moving.

    Args:
      x: X coordinate on the screen.
      y: Y coordinate on the screen.
      width: Width of the area to scan.
      height: Height of the area to scan.

    Returns:
      True, if pixel color in area is changing, or
      False otherwise.
    """
    color_a = self._GetAreaPixelColor(x, y, width, height)
    def _HasColorChanged():
      color_b = self._GetAreaPixelColor(x, y, width, height)
      return color_a != color_b

    return self.WaitUntil(_HasColorChanged, timeout=6,
                          retry_sleep=2, expect_retval=True)

  def _IsGetPixelSupported(self):
    """Check if get pixel is supported.

    Returns:
      True, if get pixel is supported, or
      False otherwise.
    """
    return pyauto.PyUITest.IsWin()

  def _GetAreaPixelColor(self, x, y, width, height):
    """Get an area of pixel color and return a list of color code values.

    Args:
      x: X coordinate on the screen.
      y: Y coordinate on the screen.
      width: Width of the area to scan.
      height: Height of the area to scan.

    Returns:
      A list containing color codes.
    """
    if pyauto.PyUITest.IsMac():
      pass  # TODO(chrisphan): Do Mac.
    elif pyauto.PyUITest.IsWin():
      return self._GetAreaPixelColorWin(x, y, width, height)
    elif pyauto.PyUITest.IsLinux():
      pass  # TODO(chrisphan): Do Linux.
    return None

  def _GetAreaPixelColorWin(self, x, y, width, height):
    """Get an area of pixel color for Windows and return a list.

    Args:
      x: X coordinate on the screen.
      y: Y coordinate on the screen.
      width: Width of the area to scan.
      height: Height of the area to scan.

    Returns:
      A list containing color codes.
    """
    colors = []
    hdc = ctypes.windll.user32.GetDC(0)
    for x_pos in xrange(x, x + width, 1):
      for y_pos in xrange(y, y + height, 1):
        color = ctypes.windll.gdi32.GetPixel(hdc, x_pos, y_pos)
        colors.append(color)
    return colors

  def _GetPixel(self, x, y):
    """Get pixel color at coordinate x and y.

    Args:
      x: X coordinate on the screen.
      y: Y coordinate on the screen.

    Returns:
      An integer color code.
    """
    if pyauto.PyUITest.IsMac():
      pass  # TODO(chrisphan): Do Mac.
    elif pyauto.PyUITest.IsWin():
      return self._GetPixelWin(x, y)
    elif pyauto.PyUITest.IsLinux():
      pass  # TODO(chrisphan): Do Linux.
    return None

  def _GetPixelWin(self, x, y):
    """Get pixel color at coordinate x and y for Windows

    Args:
      x: X coordinate on the screen.
      y: Y coordinate on the screen.

    Returns:
      An integer color code.
    """
    hdc = ctypes.windll.user32.GetDC(0)
    color = ctypes.windll.gdi32.GetPixel(hdc, x, y)
    return color

  def _CheckForCrashes(self, last_action=None, last_action_param=None):
    """Check for any browser/tab crashes and hangs.

    Args:
      last_action: Specify action taken before checking for crashes.
      last_action_param: Parameter for last action.
    """
    self.assertTrue(self.GetBrowserWindowCount(),
                    msg='Browser crashed, no window is open.')

    info = self.GetBrowserInfo()
    breakpad_folder = info['properties']['DIR_CRASH_DUMPS']
    old_dmp_files = glob.glob(os.path.join(breakpad_folder, '*.dmp'))

    # Verify there're no crash dump files.
    for dmp_file in glob.glob(os.path.join(breakpad_folder, '*.dmp')):
      self.assertTrue(dmp_file in old_dmp_files,
                      msg='Crash dump %s found' % dmp_file)

    # Check for any crashed tabs.
    tabs = info['windows'][0]['tabs']
    for tab in tabs:
      if tab['url'] != 'about:blank':
        if not self.GetDOMValue('document.body.innerHTML', tab['index']):
          self.fail(msg='Tab crashed on %s' % tab['url'])

    # TODO(chrisphan): Check for tab hangs and browser hangs.
    # TODO(chrisphan): Handle specific action: close browser, close tab.
    if last_action == 'close tab':
      pass
    elif last_action == 'close browser':
      pass
    else:
      pass

  def _GetPlatformArchitecture(self):
    """Get platform architecture.

    Args:
      last_action: Last action taken before checking for crashes.
      last_action_param: Parameter for last action.

    Returns:
      A string representing the platform architecture.
    """
    if pyauto.PyUITest.IsWin():
      if os.environ['PROGRAMFILES'] == 'C:\\Program Files (x86)':
        return '64bit'
      else:
        return '32bit'
    elif pyauto.PyUITest.IsMac() or pyauto.PyUITest.IsLinux():
      if platform.machine() == 'x86_64':
        return '64bit'
      else:
        return '32bit'
    return '32bit'

  def _HasFile(self, pattern, root=os.curdir):
    """Check if a file matching the specified pattern exists in a directory.

    Args:
      pattern: Pattern of file name.
      root: Directory to start looking.

    Returns:
      True, if root contains the file name pattern, or
      False otherwise.
    """
    return len(glob.glob(os.path.join(root, pattern)))

  def _HasPathInTree(self, pattern, is_file, root=os.curdir):
    """Recursively checks if a file/directory matching a pattern exists.

    Args:
      pattern: Pattern of file or directory name.
      is_file: True if looking for file, or False if looking for directory.
      root: Directory to start looking.

    Returns:
      True, if root contains the directory name pattern, or
      False otherwise.
    """
    for path, dirs, files in os.walk(os.path.abspath(root)):
      if is_file:
        if len(fnmatch.filter(files, pattern)):
          return True
      else:
        if len(fnmatch.filter(dirs, pattern)):
          return True
    return False

  def _GetDirectoryPath(self, pattern, root=os.curdir):
    """Get the path of a directory in another directory.

    Args:
      pattern: Pattern of directory name.
      root: Directory to start looking.

    Returns:
      A string of the path.
    """
    for path, dirs, files in os.walk(os.path.abspath(root)):
        result = fnmatch.filter(dirs, pattern)
        if result:
            return os.path.join(path, result[0])
    return None

  def _HasAllSystemRequirements(self):
    """Verify NaCl SDK installation system requirements.

    Returns:
        True, if system passed requirements, or
        False otherwise.
    """
    # Check python version.
    if sys.version_info[0:2] < (2, 6):
      return False

    # Check OS requirements.
    if pyauto.PyUITest.IsMac():
      mac_min_version = version.StrictVersion('10.6')
      mac_version = version.StrictVersion(platform.mac_ver()[0])
      if mac_version < mac_min_version:
        return False
    elif pyauto.PyUITest.IsWin():
      if not (self.IsWin7() or self.IsWinVista() or self.IsWinXP()):
        return False
    elif pyauto.PyUITest.IsLinux():
      pass  # TODO(chrisphan): Check Lin requirements.
    else:
      return False

    # Check for Chrome version compatibility.
    # NaCl supports Chrome 10 and higher builds.
    min_required_chrome_build = self._settings['min_required_chrome_build']
    browser_info = self.GetBrowserInfo()
    chrome_version =  browser_info['properties']['ChromeVersion']
    chrome_build = int(chrome_version.split('.')[0])
    return chrome_build >= min_required_chrome_build

  def _DownloadNaClSDK(self):
    """Download NaCl SDK."""
    self._temp_dir = tempfile.mkdtemp()
    if pyauto.PyUITest.IsWin():
      dl_file = urllib2.urlopen(self._settings['release_win_sdk_url'])
      file_path = os.path.join(self._temp_dir, 'naclsdk_win.exe')
    elif pyauto.PyUITest.IsMac():
      dl_file = urllib2.urlopen(self._settings['release_mac_sdk_url'])
      file_path = os.path.join(self._temp_dir, 'naclsdk_mac.tgz')
    elif pyauto.PyUITest.IsLinux():
      dl_file = urllib2.urlopen(self._settings['release_lin_sdk_url'])
      file_path = os.path.join(self._temp_dir, 'naclsdk_linux.tgz')
    else:
      self.fail(msg='NaCl SDK does not support this OS.')

    try:
      f = open(file_path, 'wb')
      f.write(dl_file.read())
    except IOError:
      self.fail(msg='Cannot open %s.' % file_path)
    finally:
      f.close()

  def _ExtractNaClSDK(self):
    """Extract NaCl SDK."""
    if pyauto.PyUITest.IsWin():
      source_file = os.path.join(self._temp_dir, 'naclsdk_win.exe')
      proc = subprocess.Popen([source_file, '/S', '/D=' +
                              self._extracted_sdk_path],
                              stdout=subprocess.PIPE)
      proc.communicate()
      if not os.listdir(self._extracted_sdk_path):
        self.fail(msg='Failed to extract NaCl SDK.')
    elif pyauto.PyUITest.IsMac():
      source_file = os.path.join(self._temp_dir, 'naclsdk_mac.tgz')
      tar = tarfile.open(source_file, 'r')
      tar.extractall(self._extracted_sdk_path)
    elif pyauto.PyUITest.IsLinux():
      source_file = os.path.join(self._temp_dir, 'naclsdk_linux.tgz')
      tar = tarfile.open(source_file, 'r')
      tar.extractall(self._extracted_sdk_path)
    else:
      self.fail(msg='NaCl SDK does not support this OS.')

  def _IsURLAlive(self, url):
    """Test if URL is alive."""
    try:
      urllib2.urlopen(url)
    except:
      return False
    return True

  def _CloseHTTPServer(self, proc=None):
    """Close HTTP server.

    Args:
      proc: Process that opened the HTTP server.
      proc is None when there is no pointer to HTTP server process.
    """
    if not self._IsURLAlive('http://localhost:5103'):
      return
    response = urllib2.urlopen('http://localhost:5103')
    html = response.read()
    if not 'Native Client' in html:
      self.fail(msg='Port 5103 is in use.')

    urllib2.urlopen('http://localhost:5103?quit=1')
    success = self.WaitUntil(
        lambda: self._IsURLAlive('http://localhost:5103'),
        timeout=30, retry_sleep=1, expect_retval=False)
    if not success:
      if not proc:
        self.fail(msg='Failed to close HTTP server.')
      else:
        if proc.poll() == None:
          try:
            proc.kill()
          except:
            self.fail(msg='Failed to close HTTP server')

  def _SearchNaClSDKTarFile(self, search_list, source_file):
    """Search NaCl SDK tar file for example files and directories.

    Args:
      search_list: A list of strings, representing file and
                   directory names for which to search.
      source_file: The string name of an NaCl SDK tar file.
    """
    tar = tarfile.open(source_file, 'r')

    # Look for files and directories in examples.
    files = copy.deepcopy(search_list)
    for tar_info in tar:
      file_name = tar_info.name
      if tar_info.isdir() and not file_name.endswith('/'):
        file_name = file_name + '/'

      for name in files:
        if file_name.find('examples/' + name):
          files.remove(name)
      if not files:
        break

    tar.close()

    self.assertEqual(len(files), 0,
                     msg='Missing files or directories: %s' %
                         ', '.join(map(str, files)))

  def _SearchNaClSDKFileWindows(self, search_list):
    """Search NaCl SDK file for example files and directories in Windows.

    Args:
      search_list: A list of strings, representing file and
                   directory names for which to search.
    """
    missing_items = []
    for name in search_list:
      is_file = name.find('/') < 0
      if not is_file:
        name = name.replace('/', '')
      if not self._HasPathInTree(name, is_file, self._extracted_sdk_path):
        missing_items.append(name)
    self.assertEqual(len(missing_items), 0,
                     msg='Missing files or directories: %s' %
                         ', '.join(map(str, missing_items)))

  def _EnableNaClPlugin(self):
    """Enable NaCl plugin."""
    nacl_plugin = (self.GetPluginsInfo().PluginForName('Chrome NaCl') or
        self.GetPluginsInfo().PluginForName('Native Client'))
    if not nacl_plugin:
      self.fail(msg='No NaCl plugin found.')
    self.EnablePlugin(nacl_plugin[0]['path'])

    self.NavigateToURL('about:flags')

    js_code = """
      chrome.send('enableFlagsExperiment', ['enable-nacl', 'true']);
      requestFlagsExperimentsData();
      window.domAutomationController.send('done');
    """
    self.ExecuteJavascript(js_code)
    self.RestartBrowser(False)

    self.NavigateToURL('about://version')
    self.assertEqual(self.FindInPage('--enable-nacl')['match_count'], 1,
                     msg='Missing expected Chrome flag --enable-nacl.')


if __name__ == '__main__':
  pyauto_functional.Main()
