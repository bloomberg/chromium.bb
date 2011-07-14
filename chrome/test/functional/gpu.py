#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

import pyauto_functional  # Must be imported before pyauto
import pyauto


class GpuTest(pyauto.PyUITest):
  """GPU Tests Runner."""

  def _GetGpuPID(self):
    """Fetch the pid of the GPU process."""
    child_processes = self.GetBrowserInfo()['child_processes']
    for x in child_processes:
       if x['type'] == 'GPU':
         return x['pid']
    return None

  def _IsHardwareAccelerated(self, feature):
    """Check if gpu is enabled in the machine before running any tests."""
    self.NavigateToURL('about:gpu')
    search = feature + ': Hardware accelerated'
    find_result = self.FindInPage(search)['match_count']
    if find_result:
      # about:gpu page starts a gpu process.
      # Make sure to kill the GPU process after navigating to about:gpu page
      pid = self._GetGpuPID()
      self.Kill(pid)
      return True
    else:
      logging.warn('Hardware acceleration not available')
      return False

  def _VerifyGPUProcessOnPage(self, url):
    url = self.GetFileURLForDataPath('pyauto_private', 'gpu', url)
    self.NavigateToURL(url)
    self.assertTrue(self.WaitUntil(
      lambda: self._GetGpuPID() is not None), msg='No process for GPU')

  def testNoGpuProcessOnStart(self):
    """Verify that no gpu process is spawned when first starting browser."""
    self.AppendTab(pyauto.GURL('about:blank'))
    self.assertFalse(self._GetGpuPID())

  def test2dCanvas(self):
    """Verify that gpu process is spawned when viewing a 2D canvas."""
    if self._IsHardwareAccelerated('Canvas'):
      self._VerifyGPUProcessOnPage('CanvasDemo.html')

  def test3dCss(self):
    """Verify that gpu process is spawned when viewing a 3D CSS page."""
    if self._IsHardwareAccelerated('3D CSS'):
      self._VerifyGPUProcessOnPage('3dCss.html')

  def testCompositing(self):
    """Verify gpu process in compositing example."""
    if self._IsHardwareAccelerated('Compositing'):
      self._VerifyGPUProcessOnPage('WebGLTeapot.html')

  def testWebGL(self):
    """Verify that gpu process is spawned in webgl example."""
    if self._IsHardwareAccelerated('WebGL'):
      self._VerifyGPUProcessOnPage('WebGLField.html')

  def testGpuWithVideo(self):
    """Verify that gpu process is started when viewing video."""
    if self._IsHardwareAccelerated('WebGL'):
      self._VerifyGPUProcessOnPage('color2.ogv')

  def testSingleGpuProcess(self):
    """Verify there's only one gpu process shared across all uses."""
    if self._IsHardwareAccelerated('WebGL'):
      url = self.GetFileURLForDataPath('pyauto_private',
                                       'gpu', 'WebGLField.html')
      self.AppendTab(pyauto.GURL(url))
      # Open a new window.
      self.OpenNewBrowserWindow(True)
      self.NavigateToURL(url, 1, 0)
      # Open a new incognito window.
      self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
      self.NavigateToURL(url, 1, 0)
      # Verify there's only 1 gpu process.
      gpu_process_count = 0
      for x in self.GetBrowserInfo()['child_processes']:
        if x['type'] == 'GPU':
          gpu_process_count += 1
      self.assertEqual(1, gpu_process_count)


if __name__ == '__main__':
  pyauto_functional.Main()