#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from empty_dir_file_system import EmptyDirFileSystem
from fake_fetchers import ConfigureFakeFetchers
from host_file_system_creator import HostFileSystemCreator
from patch_servlet import PatchServlet
from render_servlet import RenderServlet
from server_instance import ServerInstance
from servlet import Request
from test_branch_utility import TestBranchUtility
from test_util import DisableLogging

_ALLOWED_HOST = 'https://chrome-apps-doc.appspot.com'

class _RenderServletDelegate(RenderServlet.Delegate):
  def CreateServerInstance(self):
    return ServerInstance.ForLocal()

class _PatchServletDelegate(RenderServlet.Delegate):
  def CreateAppSamplesFileSystem(self, object_store_creator):
    return EmptyDirFileSystem()

  def CreateBranchUtility(self, object_store_creator):
    return TestBranchUtility.CreateWithCannedData()

  def CreateHostFileSystemCreator(self, object_store_creator):
    return HostFileSystemCreator.ForLocal(object_store_creator)

class PatchServletTest(unittest.TestCase):
  def setUp(self):
    ConfigureFakeFetchers()

  def _RenderWithPatch(self, path, issue):
    path_with_issue = '%s/%s' % (issue, path)
    return PatchServlet(Request.ForTest(path_with_issue, host=_ALLOWED_HOST),
                        _PatchServletDelegate()).Get()

  def _RenderWithoutPatch(self, path):
    return RenderServlet(Request.ForTest(path, host=_ALLOWED_HOST),
                         _RenderServletDelegate()).Get()

  def _RenderAndCheck(self, path, issue, expected_equal):
    patched_response = self._RenderWithPatch(path, issue)
    unpatched_response = self._RenderWithoutPatch(path)
    patched_response.headers.pop('cache-control', None)
    unpatched_response.headers.pop('cache-control', None)
    patched_content = patched_response.content.ToString().replace(
        '/_patch/%s/' % issue, '/')
    unpatched_content = unpatched_response.content.ToString()

    self.assertEqual(patched_response.status, unpatched_response.status)
    self.assertEqual(patched_response.headers, unpatched_response.headers)
    if expected_equal:
      self.assertEqual(patched_content, unpatched_content)
    else:
      self.assertNotEqual(patched_content, unpatched_content)

  def _RenderAndAssertEqual(self, path, issue):
    self._RenderAndCheck(path, issue, True)

  def _RenderAndAssertNotEqual(self, path, issue):
    self._RenderAndCheck(path, issue, False)

  @DisableLogging('warning')
  def _AssertNotFound(self, path, issue):
    response = self._RenderWithPatch(path, issue)
    self.assertEqual(response.status, 404,
        'Path %s with issue %s should have been removed for %s.' % (
            path, issue, response))

  def _AssertOk(self, path, issue):
    response = self._RenderWithPatch(path, issue)
    self.assertEqual(response.status, 200,
        'Failed to render path %s with issue %s.' % (path, issue))
    self.assertTrue(len(response.content.ToString()) > 0,
        'Rendered result for path %s with issue %s should not be empty.' %
        (path, issue))

  def _AssertRedirect(self, path, issue, redirect_path):
    response = self._RenderWithPatch(path, issue)
    self.assertEqual(302, response.status)
    self.assertEqual('/_patch/%s/%s' % (issue, redirect_path),
                     response.headers['Location'])

  def testRender(self):
    # '_patch' is not included in paths below because it's stripped by Handler.
    issue = '14096030'

    # extensions_sidenav.json is modified in the patch.
    self._RenderAndAssertNotEqual('extensions/index.html', issue)

    # apps_sidenav.json is not patched.
    self._RenderAndAssertEqual('apps/about_apps.html', issue)

    # extensions/runtime.html is removed in the patch, should redirect to the
    # apps version.
    self._AssertRedirect('extensions/runtime.html', issue,
                         'apps/runtime.html')

    # apps/runtime.html is not removed.
    self._RenderAndAssertEqual('apps/runtime.html', issue)

    # test_foo.html is added in the patch.
    self._AssertOk('extensions/test_foo.html', issue)

    # Invalid issue number results in a 404.
    self._AssertNotFound('extensions/index.html', '11111')

  def testXssRedirect(self):
    def is_redirect(from_host, from_path, to_url):
      response = PatchServlet(Request.ForTest(from_path, host=from_host),
                              _PatchServletDelegate()).Get()
      redirect_url, _ = response.GetRedirect()
      if redirect_url is None:
        return (False, '%s/%s did not cause a redirect' % (
            from_host, from_path))
      if redirect_url != to_url:
        return (False, '%s/%s redirected to %s not %s' % (
            from_host, from_path, redirect_url, to_url))
      return (True, '%s/%s redirected to %s' % (
          from_host, from_path, redirect_url))
    self.assertTrue(*is_redirect('http://developer.chrome.com', '12345',
                                 '%s/_patch/12345' % _ALLOWED_HOST))
    self.assertTrue(*is_redirect('http://developers.google.com', '12345',
                                 '%s/_patch/12345' % _ALLOWED_HOST))
    self.assertFalse(*is_redirect('http://chrome-apps-doc.appspot.com', '12345',
                                  None))
    self.assertFalse(*is_redirect('http://some-other-app.appspot.com', '12345',
                                  None))

if __name__ == '__main__':
  unittest.main()
