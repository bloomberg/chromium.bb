#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from empty_dir_file_system import EmptyDirFileSystem
from fake_fetchers import ConfigureFakeFetchers
from local_file_system import LocalFileSystem
from patch_servlet import PatchServlet
from render_servlet import RenderServlet
from server_instance import ServerInstance
from servlet import Request

class _RenderServletDelegate(RenderServlet.Delegate):
  def CreateServerInstanceForChannel(self, channel):
    return ServerInstance.ForLocal()

class _PatchServletDelegate(RenderServlet.Delegate):
  def CreateAppSamplesFileSystem(self, object_store_creator):
    return EmptyDirFileSystem()

  def CreateHostFileSystemForBranch(self, channel):
    return LocalFileSystem.Create()

class PatchServletTest(unittest.TestCase):
  def setUp(self):
    ConfigureFakeFetchers()

  def _RenderWithPatch(self, path, issue):
    real_path = '%s/%s' % (issue, path)
    return PatchServlet(Request.ForTest(real_path),
                        _PatchServletDelegate()).Get()

  def _RenderWithoutPatch(self, path):
    return RenderServlet(Request.ForTest(path), _RenderServletDelegate()).Get()

  def _RenderAndCheck(self, path, issue, expected_equal):
    patched_response = self._RenderWithPatch(path, issue)
    unpatched_response = self._RenderWithoutPatch(path)
    patched_response.headers.pop('cache-control', None)
    unpatched_response.headers.pop('cache-control', None)
    patched_content = patched_response.content.ToString().replace(
        '/_patch/%s/static/' % issue, '/static/')
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

  def _AssertNotFound(self, path, issue):
    self.assertEqual(self._RenderWithPatch(path, issue).status, 404,
        'Path %s with issue %s should have been removed.' % (path, issue))

  def _AssertOk(self, path, issue):
    response = self._RenderWithPatch(path, issue)
    self.assertEqual(response.status, 200,
        'Failed to render path %s with issue %s.' % (path, issue))
    self.assertTrue(len(response.content.ToString()) > 0,
        'Rendered result for path %s with issue %s should not be empty.' %
        (path, issue))

  def testRender(self):
    # '_patch' is not included in paths below because it's stripped by Handler.
    issue = '14096030'

    # extensions_sidenav.json is modified in the patch.
    self._RenderAndAssertNotEqual('extensions/index.html', issue)
    # apps_sidenav.json is not patched.
    self._RenderAndAssertEqual('apps/about_apps.html', issue)

    # extensions/runtime.html is removed in the patch.
    self._AssertNotFound('extensions/runtime.html', issue)
    # apps/runtime.html is not removed.
    self._RenderAndAssertEqual('apps/runtime.html', issue)

    # test_foo.html is added in the patch.
    self._AssertOk('extensions/test_foo.html', issue)

    # Invalid issue number results in a 404.
    self._AssertNotFound('extensions/index.html', '11111')

    # Test redirect.
    self.assertEqual(self._RenderWithPatch('extensions/',
                                          issue).headers['Location'],
                     '/_patch/%s/extensions/index.html' % issue)

if __name__ == '__main__':
  unittest.main()
