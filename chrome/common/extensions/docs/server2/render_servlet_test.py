#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from local_file_system import LocalFileSystem
from render_servlet import RenderServlet
from server_instance import ServerInstance
from servlet import Request, Response
from test_util import DisableLogging, ReadFile

class _RenderServletDelegate(RenderServlet.Delegate):
  def CreateServerInstance(self):
    return ServerInstance.ForTest(LocalFileSystem.Create())

class RenderServletTest(unittest.TestCase):
  def testExtensionAppRedirect(self):
    request = Request.ForTest('storage.html')
    self.assertEqual(
        Response.Redirect('/extensions/storage.html', permanent=False),
        RenderServlet(request, _RenderServletDelegate()).Get())

  def testChannelRedirect(self):
    request = Request.ForTest('stable/extensions/storage.html')
    self.assertEqual(
        Response.Redirect('/extensions/storage.html', permanent=True),
        RenderServlet(request, _RenderServletDelegate()).Get())

  @DisableLogging('warning')
  def testNotFound(self):
    request = Request.ForTest('extensions/not_found.html')
    response = RenderServlet(request, _RenderServletDelegate()).Get()
    self.assertEqual(404, response.status)

  def testSampleFile(self):
    sample_file = 'extensions/talking_alarm_clock/background.js'
    request = Request.ForTest('extensions/examples/%s' % sample_file)
    response = RenderServlet(request, _RenderServletDelegate()).Get()
    self.assertEqual(200, response.status)
    content_type = response.headers.get('content-type')
    self.assertTrue(content_type == 'application/javascript' or
                    content_type == 'application/x-javascript')
    self.assertEqual(ReadFile('docs/examples/%s' % sample_file),
                     response.content.ToString())

  def testSampleZip(self):
    sample_dir = 'extensions/talking_alarm_clock'
    request = Request.ForTest('extensions/examples/%s.zip' % sample_dir)
    response = RenderServlet(request, _RenderServletDelegate()).Get()
    self.assertEqual(200, response.status)
    self.assertEqual('application/zip', response.headers.get('content-type'))

  def testStaticFile(self):
    static_file = 'css/site.css'
    request = Request.ForTest('static/%s' % static_file)
    response = RenderServlet(request, _RenderServletDelegate()).Get()
    self.assertEqual(200, response.status)
    self.assertEqual('text/css', response.headers.get('content-type'))
    self.assertEqual(ReadFile('docs/static/%s' % static_file),
                     response.content.ToString())

  def testHtmlTemplate(self):
    html_file = 'extensions/storage.html'
    request = Request.ForTest(html_file)
    response = RenderServlet(request, _RenderServletDelegate()).Get()
    self.assertEqual(200, response.status)
    self.assertEqual('text/html', response.headers.get('content-type'))
    # Can't really test rendering all that well.
    self.assertTrue(len(response.content) >
                    len(ReadFile('docs/templates/public/%s' % html_file)))

if __name__ == '__main__':
  unittest.main()
