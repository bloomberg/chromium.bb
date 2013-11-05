#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from appengine_wrappers import GetAppVersion
from app_yaml_helper import AppYamlHelper
from cron_servlet import CronServlet
from empty_dir_file_system import EmptyDirFileSystem
from github_file_system_provider import GithubFileSystemProvider
from host_file_system_provider import HostFileSystemProvider
from local_file_system import LocalFileSystem
from mock_file_system import MockFileSystem
from servlet import Request
from svn_constants import JSON_PATH
from test_branch_utility import TestBranchUtility
from test_file_system import TestFileSystem
from test_util import EnableLogging, ReadFile

# NOTE(kalman): The ObjectStore created by the CronServlet is backed onto our
# fake AppEngine memcache/datastore, so the tests aren't isolated. Of course,
# if the host file systems have different identities, they will be, sort of.
class _TestDelegate(CronServlet.Delegate):
  def __init__(self, create_file_system):
    self.file_systems = []
    # A callback taking a revision and returning a file system.
    self._create_file_system = create_file_system
    self._app_version = GetAppVersion()

  def CreateBranchUtility(self, object_store_creator):
    return TestBranchUtility.CreateWithCannedData()

  def CreateHostFileSystemProvider(self,
                                  object_store_creator,
                                  max_trunk_revision=None):
    def constructor(branch=None, revision=None):
      file_system = self._create_file_system(revision)
      self.file_systems.append(file_system)
      return file_system
    return HostFileSystemProvider(object_store_creator,
                                  max_trunk_revision=max_trunk_revision,
                                  constructor_for_test=constructor)

  def CreateGithubFileSystemProvider(self, object_store_creator):
    return GithubFileSystemProvider.ForEmpty()

  def GetAppVersion(self):
    return self._app_version

  # (non-Delegate method).
  def SetAppVersion(self, app_version):
    self._app_version = app_version

class CronServletTest(unittest.TestCase):
  @EnableLogging('info')
  def testEverything(self):
    # All these tests are dependent (see above comment) so lump everything in
    # the one test.
    delegate = _TestDelegate(lambda _: MockFileSystem(LocalFileSystem.Create()))

    # Test that the cron runs successfully.
    response = CronServlet(Request.ForTest('trunk'),
                           delegate_for_test=delegate).Get()
    self.assertEqual(200, response.status)

    # Save the file systems created, start with a fresh set for the next run.
    first_run_file_systems = delegate.file_systems[:]
    delegate.file_systems[:] = []

    # When re-running, all file systems should be Stat()d the same number of
    # times, but the second round shouldn't have been re-Read() since the
    # Stats haven't changed.
    response = CronServlet(Request.ForTest('trunk'),
                           delegate_for_test=delegate).Get()
    self.assertEqual(200, response.status)

    self.assertEqual(len(first_run_file_systems), len(delegate.file_systems))
    for i, second_run_file_system in enumerate(delegate.file_systems):
      self.assertTrue(*second_run_file_system.CheckAndReset(
          read_count=0,
          stat_count=first_run_file_systems[i].GetStatCount()))

  def testSafeRevision(self):
    test_data = {
      'api': {
        '_manifest_features.json': '{}'
      },
      'docs': {
        'examples': {
          'examples.txt': 'examples.txt contents'
        },
        'server2': {
          'app.yaml': AppYamlHelper.GenerateAppYaml('2-0-8')
        },
        'static': {
          'static.txt': 'static.txt contents'
        },
        'templates': {
          'public': {
            'apps': {
              'storage.html': 'storage.html contents'
            },
            'extensions': {
              'storage.html': 'storage.html contents'
            },
          },
          'json': {
            'content_providers.json': ReadFile('%s/content_providers.json' %
                                               JSON_PATH),
            'manifest.json': '{}',
            'strings.json': '{}',
            'apps_sidenav.json': '{}',
            'extensions_sidenav.json': '{}',
          },
        }
      }
    }

    updates = []

    def app_yaml_update(version):
      return {'docs': {'server2': {
        'app.yaml': AppYamlHelper.GenerateAppYaml(version)
      }}}
    def storage_html_update(update):
      return {'docs': {'templates': {'public': {'apps': {
        'storage.html': update
      }}}}}
    def static_txt_update(update):
      return {'docs': {'static': {
        'static.txt': update
      }}}

    app_yaml_path = 'docs/server2/app.yaml'
    storage_html_path = 'docs/templates/public/apps/storage.html'
    static_txt_path = 'docs/static/static.txt'

    def create_file_system(revision=None):
      '''Creates a MockFileSystem at |revision| by applying that many |updates|
      to it.
      '''
      mock_file_system = MockFileSystem(TestFileSystem(test_data))
      updates_for_revision = (
          updates if revision is None else updates[:int(revision)])
      for update in updates_for_revision:
        mock_file_system.Update(update)
      return mock_file_system

    delegate = _TestDelegate(create_file_system)
    delegate.SetAppVersion('2-0-8')

    file_systems = delegate.file_systems

    # No updates applied yet.
    CronServlet(Request.ForTest('trunk'), delegate_for_test=delegate).Get()
    self.assertEqual(AppYamlHelper.GenerateAppYaml('2-0-8'),
                     file_systems[-1].ReadSingle(app_yaml_path).Get())
    self.assertEqual('storage.html contents',
                     file_systems[-1].ReadSingle(storage_html_path).Get())

    # Apply updates to storage.html.
    updates.append(storage_html_update('interim contents'))
    updates.append(storage_html_update('new contents'))

    CronServlet(Request.ForTest('trunk'), delegate_for_test=delegate).Get()
    self.assertEqual(AppYamlHelper.GenerateAppYaml('2-0-8'),
                     file_systems[-1].ReadSingle(app_yaml_path).Get())
    self.assertEqual('new contents',
                     file_systems[-1].ReadSingle(storage_html_path).Get())

    # Apply several updates to storage.html and app.yaml. The file system
    # should be pinned at the version before app.yaml changed.
    updates.append(storage_html_update('stuck here contents'))

    double_update = storage_html_update('newer contents')
    double_update.update(app_yaml_update('2-0-10'))
    updates.append(double_update)

    updates.append(storage_html_update('never gonna reach here'))

    CronServlet(Request.ForTest('trunk'), delegate_for_test=delegate).Get()
    self.assertEqual(AppYamlHelper.GenerateAppYaml('2-0-8'),
                     file_systems[-1].ReadSingle(app_yaml_path).Get())
    self.assertEqual('stuck here contents',
                     file_systems[-1].ReadSingle(storage_html_path).Get())

    # Further pushes to storage.html will keep it pinned.
    updates.append(storage_html_update('y u not update!'))

    CronServlet(Request.ForTest('trunk'), delegate_for_test=delegate).Get()
    self.assertEqual(AppYamlHelper.GenerateAppYaml('2-0-8'),
                     file_systems[-1].ReadSingle(app_yaml_path).Get())
    self.assertEqual('stuck here contents',
                     file_systems[-1].ReadSingle(storage_html_path).Get())

    # Likewise app.yaml.
    updates.append(app_yaml_update('2-1-0'))

    CronServlet(Request.ForTest('trunk'), delegate_for_test=delegate).Get()
    self.assertEqual(AppYamlHelper.GenerateAppYaml('2-0-8'),
                     file_systems[-1].ReadSingle(app_yaml_path).Get())
    self.assertEqual('stuck here contents',
                     file_systems[-1].ReadSingle(storage_html_path).Get())

    # And updates to other content won't happen either.
    updates.append(static_txt_update('important content!'))

    CronServlet(Request.ForTest('trunk'), delegate_for_test=delegate).Get()
    self.assertEqual(AppYamlHelper.GenerateAppYaml('2-0-8'),
                     file_systems[-1].ReadSingle(app_yaml_path).Get())
    self.assertEqual('stuck here contents',
                     file_systems[-1].ReadSingle(storage_html_path).Get())
    self.assertEqual('static.txt contents',
                     file_systems[-1].ReadSingle(static_txt_path).Get())

    # Lastly - when the app version changes, everything should no longer be
    # pinned.
    delegate.SetAppVersion('2-1-0')
    CronServlet(Request.ForTest('trunk'), delegate_for_test=delegate).Get()
    self.assertEqual(AppYamlHelper.GenerateAppYaml('2-1-0'),
                     file_systems[-1].ReadSingle(app_yaml_path).Get())
    self.assertEqual('y u not update!',
                     file_systems[-1].ReadSingle(storage_html_path).Get())
    self.assertEqual('important content!',
                     file_systems[-1].ReadSingle(static_txt_path).Get())

if __name__ == '__main__':
  unittest.main()
