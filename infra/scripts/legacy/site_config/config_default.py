# Copyright 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Seeds a number of variables defined in chromium_config.py.

The recommended way is to fork this file and use a custom DEPS forked from
config/XXX/DEPS with the right configuration data."""

import os
import re
import socket


SERVICE_ACCOUNTS_PATH = '/creds/service_accounts'


class classproperty(object):
  """A decorator that allows is_production_host to only to be defined once."""
  def __init__(self, getter):
    self.getter = getter
  def __get__(self, instance, owner):
    return self.getter(owner)


class Master(object):
  # Repository URLs used by the SVNPoller and 'gclient config'.
  server_url = 'http://src.chromium.org'
  repo_root = '/svn'
  git_server_url = 'https://chromium.googlesource.com'

  # External repos.
  googlecode_url = 'http://%s.googlecode.com/svn'
  sourceforge_url = 'https://svn.code.sf.net/p/%(repo)s/code'
  googlecode_revlinktmpl = 'https://code.google.com/p/%s/source/browse?r=%s'

  # Directly fetches from anonymous Blink svn server.
  webkit_root_url = 'http://src.chromium.org/blink'
  nacl_trunk_url = 'http://src.chromium.org/native_client/trunk'

  llvm_url = 'http://llvm.org/svn/llvm-project'

  # Perf Dashboard upload URL.
  dashboard_upload_url = 'https://chromeperf.appspot.com'

  # Actually for Chromium OS slaves.
  chromeos_url = git_server_url + '/chromiumos.git'

  # Default domain for emails to come from and
  # domains to which emails can be sent.
  master_domain = 'example.com'
  permitted_domains = ('example.com',)

  # Your smtp server to enable mail notifications.
  smtp = 'smtp'

  # By default, bot_password will be filled in by config.GetBotPassword().
  bot_password = None

  # Fake urls to make various factories happy.
  trunk_internal_url = None
  trunk_internal_url_src = None
  slave_internal_url = None
  git_internal_server_url = None
  syzygy_internal_url = None
  v8_internal_url = None


  class Base(object):
    """Master base template.
    Contains stubs for variables that all masters must define."""

    # Base service offset for 'master_port'
    MASTER_PORT = 2
    # Base service offset for 'slave_port'
    SLAVE_PORT = 3
    # Base service offset for 'master_port_alt'
    MASTER_PORT_ALT = 4
    # Base service offset for 'try_job_port'
    TRY_JOB_PORT = 5

    # A BuildBucket bucket to poll.
    buildbucket_bucket = None

    # Master address. You should probably copy this file in another svn repo
    # so you can override this value on both the slaves and the master.
    master_host = 'localhost'
    @classproperty
    def current_host(cls):
      return socket.getfqdn()
    @classproperty
    def in_production(cls):
      return re.match(r'master.*\.golo\.chromium\.org', cls.current_host)
    # Only report that we are running on a master if the master_host (even when
    # master_host is overridden by a subclass) is the same as the current host.
    @classproperty
    def is_production_host(cls):
      return cls.current_host == cls.master_host

    # 'from:' field for emails sent from the server.
    from_address = 'nobody@example.com'
    # Additional email addresses to send gatekeeper (automatic tree closage)
    # notifications. Unnecessary for experimental masters and try servers.
    tree_closing_notification_recipients = []

    @classproperty
    def master_port(cls):
      return cls._compose_port(cls.MASTER_PORT)

    @classproperty
    def slave_port(cls):
      # Which port slaves use to connect to the master.
      return cls._compose_port(cls.SLAVE_PORT)

    @classproperty
    def master_port_alt(cls):
      # The alternate read-only page. Optional.
      return cls._compose_port(cls.MASTER_PORT_ALT)

    @classproperty
    def try_job_port(cls):
      return cls._compose_port(cls.TRY_JOB_PORT)

    @classmethod
    def _compose_port(cls, service):
      """Returns: The port number for 'service' from the master's static config.

      Port numbers are mapped of the form:
      XYYZZ
      || \__The last two digits identify the master, e.g. master.chromium
      |\____The second and third digits identify the master host, e.g.
      |     master1.golo
      \_____The first digit identifies the port type, e.g. master_port

      If any configuration is missing (incremental migration), this method will
      return '0' for that query, indicating no port.
      """
      return (
          (service * 10000) + # X
          (cls.master_port_base * 100) + # YY
          cls.master_port_id) # ZZ

    service_account_file = None

    @classproperty
    def service_account_path(cls):
      if cls.service_account_file is None:
        return None
      return os.path.join(SERVICE_ACCOUNTS_PATH, cls.service_account_file)

  ## Per-master configs.

  class Master1(Base):
    """Chromium master."""
    master_host = 'master1.golo.chromium.org'
    master_port_base = 1
    from_address = 'buildbot@chromium.org'
    tree_closing_notification_recipients = [
        'chromium-build-failure@chromium-gatekeeper-sentry.appspotmail.com']
    base_app_url = 'https://chromium-status.appspot.com'
    tree_status_url = base_app_url + '/status'
    store_revisions_url = base_app_url + '/revisions'
    last_good_url = base_app_url + '/lkgr'
    last_good_blink_url = 'http://blink-status.appspot.com/lkgr'

  class Master2(Base):
    """Legacy ChromeOS master."""
    master_host = 'master2.golo.chromium.org'
    master_port_base = 2
    tree_closing_notification_recipients = [
        'chromeos-build-failures@google.com']
    from_address = 'buildbot@chromium.org'

  class Master2a(Base):
    """Chromeos master."""
    master_host = 'master2a.golo.chromium.org'
    master_port_base = 15
    tree_closing_notification_recipients = [
        'chromeos-build-failures@google.com']
    from_address = 'buildbot@chromium.org'

  class Master3(Base):
    """Client master."""
    master_host = 'master3.golo.chromium.org'
    master_port_base = 3
    tree_closing_notification_recipients = []
    from_address = 'buildbot@chromium.org'

  class Master4(Base):
    """Try server master."""
    master_host = 'master4.golo.chromium.org'
    master_port_base = 4
    tree_closing_notification_recipients = []
    from_address = 'tryserver@chromium.org'
    code_review_site = 'https://codereview.chromium.org'

  class Master4a(Base):
    """Try server master."""
    master_host = 'master4a.golo.chromium.org'
    master_port_base = 14
    tree_closing_notification_recipients = []
    from_address = 'tryserver@chromium.org'
    code_review_site = 'https://codereview.chromium.org'

  ## Native Client related

  class NaClBase(Master3):
    """Base class for Native Client masters."""
    tree_closing_notification_recipients = ['bradnelson@chromium.org']
    base_app_url = 'https://nativeclient-status.appspot.com'
    tree_status_url = base_app_url + '/status'
    store_revisions_url = base_app_url + '/revisions'
    last_good_url = base_app_url + '/lkgr'
    perf_base_url = 'http://build.chromium.org/f/client/perf'

  ## ChromiumOS related

  class ChromiumOSBase(Master2):
    """Legacy base class for ChromiumOS masters"""
    base_app_url = 'https://chromiumos-status.appspot.com'
    tree_status_url = base_app_url + '/status'
    store_revisions_url = base_app_url + '/revisions'
    last_good_url = base_app_url + '/lkgr'

  class ChromiumOSBase2a(Master2a):
    """Base class for ChromiumOS masters"""
    base_app_url = 'https://chromiumos-status.appspot.com'
    tree_status_url = base_app_url + '/status'
    store_revisions_url = base_app_url + '/revisions'
    last_good_url = base_app_url + '/lkgr'
