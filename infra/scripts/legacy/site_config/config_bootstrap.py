# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Site configuration information that is sufficient to configure a slave,
without loading any buildbot or twisted code.
"""

import inspect
import os

BASE_DIR = os.path.dirname(os.path.abspath(__file__))

# Override config_default with a config_private file.
BASE_MASTERS = []
try:
  import config_private # pylint: disable=F0401
  BASE_MASTERS += [config_private.Master, config_private.PublicMaster]
except ImportError:
  import config_default as config_private # pylint: disable=W0403
  BASE_MASTERS += [config_private.Master,]


class Master(config_private.Master):
  """Buildbot master configuration options."""

  trunk_url = (config_private.Master.server_url +
               config_private.Master.repo_root + '/trunk')

  webkit_trunk_url = (config_private.Master.webkit_root_url + '/trunk')

  trunk_url_src = config_private.Master.git_server_url + '/chromium/src.git'
  trunk_url_tools = trunk_url + '/tools'
  nacl_url = config_private.Master.nacl_trunk_url + '/src/native_client'
  nacl_sdk_root_url = 'https://nativeclient-sdk.googlecode.com/svn'
  nacl_ports_trunk_url = 'https://naclports.googlecode.com/svn/trunk'
  nacl_ports_url = nacl_ports_trunk_url + '/src'
  gears_url = 'http://gears.googlecode.com/svn/trunk'
  gyp_trunk_url = 'http://gyp.googlecode.com/svn/trunk'
  branch_url = (config_private.Master.server_url +
                config_private.Master.repo_root + '/branches')
  merge_branch_url = branch_url + '/chrome_webkit_merge_branch'
  merge_branch_url_src = merge_branch_url + '/src'

  v8_url = 'http://v8.googlecode.com/svn'
  v8_branch_url = (v8_url + '/branches')
  v8_bleeding_edge = v8_branch_url + '/bleeding_edge'
  v8_trunk = v8_url + '/trunk'
  es5conform_root_url = "https://es5conform.svn.codeplex.com/svn/"
  es5conform_revision = 62998

  dart_url = config_private.Master.googlecode_url % 'dart'
  dart_bleeding = dart_url + '/branches/bleeding_edge'
  dart_trunk = dart_url + '/trunk'

  oilpan_url = (config_private.Master.webkit_root_url + '/branches/oilpan')

  skia_url = 'http://skia.googlecode.com/svn/'

  syzygy_url = 'http://sawbuck.googlecode.com/svn/'

  webrtc_url = config_private.Master.googlecode_url % 'webrtc'
  libyuv_url = 'http://libyuv.googlecode.com/svn'

  # Default target platform if none was given to the factory.
  default_platform = 'win32'

  # Used by the waterfall display.
  project_url = 'http://www.chromium.org'

  # Base URL for perf test results.
  perf_base_url = 'http://build.chromium.org/f/chromium/perf'

  # Suffix for perf URL.
  perf_report_url_suffix = 'report.html?history=150'

  # Directory in which to save perf-test output data files.
  perf_output_dir = '~/www/perf'

  # URL pointing to builds and test results.
  archive_url = 'http://build.chromium.org/buildbot'

  # The test results server to upload our test results.
  test_results_server = 'test-results.appspot.com'

  # File in which to save a list of graph names.
  perf_graph_list = 'graphs.dat'

  # Magic step return code inidicating "warning(s)" rather than "error".
  retcode_warnings = 88

  @staticmethod
  def GetBotPassword():
    """Returns the slave password retrieved from a local file, or None.

    The slave password is loaded from a local file next to this module file, if
    it exists.  This is a function rather than a variable so it's not called
    when it's not needed.

    We can't both make this a property and also keep it static unless we use a
    <metaclass, which is overkill for this usage.
    """
    # Note: could be overriden by config_private.
    if not getattr(Master, 'bot_password', None):
      # If the bot_password has been requested, the file is required to exist
      # if not overriden in config_private.
      bot_password_path = os.path.join(BASE_DIR, '.bot_password')
      Master.bot_password = open(bot_password_path).read().strip('\n\r')
    return Master.bot_password

  @staticmethod
  def _extract_masters(master):
    return [v for v in master.__dict__.itervalues()
            if (inspect.isclass(v) and
                issubclass(v, config_private.Master.Base) and
                v != config_private.Master.Base)]

  @classmethod
  def get_base_masters(cls):
    masters = []
    for base_master in BASE_MASTERS:
      masters += cls._extract_masters(base_master)
    return masters

  @classmethod
  def get_all_masters(cls):
    return cls._extract_masters(cls)
