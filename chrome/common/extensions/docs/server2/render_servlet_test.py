#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from extensions_paths import EXAMPLES, PUBLIC_TEMPLATES, STATIC_DOCS
from local_file_system import LocalFileSystem
from render_servlet import RenderServlet
from server_instance import ServerInstance
from servlet import Request, Response
from test_util import ReadFile


class _RenderServletDelegate(RenderServlet.Delegate):
  def CreateServerInstance(self):
    return ServerInstance.ForTest(LocalFileSystem.Create())


class RenderServletTest(unittest.TestCase):
  def _Render(self, path):
    return RenderServlet(Request.ForTest(path),
                         _RenderServletDelegate()).Get()

  def testExtensionAppRedirect(self):
    self.assertEqual(
        Response.Redirect('/extensions/storage.html', permanent=False),
        self._Render('storage.html'))

  def testChannelRedirect(self):
    self.assertEqual(
        Response.Redirect('/extensions/storage.html', permanent=True),
        self._Render('stable/extensions/storage.html'))

  def testNotFound(self):
    def create_404_response(real_path):
      real_404 = self._Render(real_path)
      self.assertEqual(200, real_404.status)
      real_404.status = 404
      return real_404

    root_404 = create_404_response('404.html')
    extensions_404 = create_404_response('extensions/404.html')
    apps_404 = create_404_response('apps/404.html')
    # Note: would test that root_404 != extensions and apps but it's not
    # necessarily true.
    self.assertNotEqual(extensions_404, apps_404)

    self.assertEqual(root_404, self._Render('not_found.html'))
    self.assertEqual(root_404, self._Render('not_found/not_found.html'))

    self.assertEqual(extensions_404, self._Render('extensions/not_found.html'))
    self.assertEqual(
        extensions_404, self._Render('extensions/manifest/not_found.html'))
    self.assertEqual(
        extensions_404,
        self._Render('extensions/manifest/not_found/not_found.html'))

    self.assertEqual(apps_404, self._Render('apps/not_found.html'))
    self.assertEqual(apps_404, self._Render('apps/manifest/not_found.html'))
    self.assertEqual(
        apps_404, self._Render('apps/manifest/not_found/not_found.html'))

  def testSampleFile(self):
    sample_file = 'extensions/talking_alarm_clock/background.js'
    response = self._Render('extensions/examples/%s' % sample_file)
    self.assertEqual(200, response.status)
    self.assertTrue(response.headers['Content-Type'] in (
        'application/javascript; charset=utf-8',
        'application/x-javascript; charset=utf-8'))
    self.assertEqual(ReadFile('%s/%s' % (EXAMPLES, sample_file)),
                     response.content.ToString())

  def testSampleZip(self):
    sample_dir = 'extensions/talking_alarm_clock'
    response = self._Render('extensions/examples/%s.zip' % sample_dir)
    self.assertEqual(200, response.status)
    self.assertEqual('application/zip', response.headers['Content-Type'])

  def testStaticFile(self):
    static_file = 'css/site.css'
    response = self._Render('static/%s' % static_file)
    self.assertEqual(200, response.status)
    self.assertEqual('text/css; charset=utf-8',
                     response.headers['Content-Type'])
    self.assertEqual(ReadFile('%s/%s' % (STATIC_DOCS, static_file)),
                     response.content.ToString())

  def testHtmlTemplate(self):
    html_file = 'extensions/storage.html'
    response = self._Render(html_file)
    self.assertEqual(200, response.status)
    self.assertEqual('text/html; charset=utf-8',
                     response.headers.get('Content-Type'))
    # Can't really test rendering all that well.
    self.assertTrue(len(response.content) >
                    len(ReadFile('%s/%s' % (PUBLIC_TEMPLATES, html_file))))

  def testDevelopersGoogleComRedirect(self):
    def assert_redirect(request_path):
      response = self._Render(request_path)
      self.assertEqual(('//developers.google.com/chrome', False),
                       response.GetRedirect())
    assert_redirect('')
    assert_redirect('index.html')

  def testIndexRedirect(self):
    response = self._Render('extensions')
    self.assertEqual(('/extensions/index.html', False),
                     response.GetRedirect())

  def testOtherRedirectsJsonRedirect(self):
    response = self._Render('apps/webview_tag.html')
    self.assertEqual(('/apps/tags/webview.html', False),
                     response.GetRedirect())


if __name__ == '__main__':
  unittest.main()
