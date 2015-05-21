# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Logic to parse and merge account databases in overlay stacks."""

from __future__ import print_function

import collections

from chromite.lib import json_lib
from chromite.lib import user_db


GROUPS_KEY = 'groups'
USERS_KEY = 'users'

USER_COMMENT_KEY = 'gecos'
USER_DEFUNCT_KEY = 'defunct'
USER_FIXED_ID_KEY = 'fixed_id'
USER_GROUP_KEY = 'group_name'
USER_HOME_KEY = 'home'
USER_ID_KEY = 'uid'
USER_NAME_KEY = 'user'
USER_PASSWORD_KEY = 'password'
USER_SHELL_KEY = 'shell'

GROUP_DEFUNCT_KEY = 'defunct'
GROUP_FIXED_ID_KEY = 'fixed_id'
GROUP_ID_KEY = 'gid'
GROUP_NAME_KEY = 'group'
GROUP_PASSWORD_KEY = 'password'
GROUP_USERS_KEY = 'users'

User = collections.namedtuple(
    'User', ('name', 'password', 'uid', 'group_name', 'description', 'home',
             'shell', 'is_fixed_id', 'is_defunct'))

Group = collections.namedtuple(
    'Group', ('name', 'password', 'gid', 'users', 'is_fixed_id', 'is_defunct'))


class AccountDatabase(object):
  """Parses, validates, and combines account databases from overlays."""

  def __init__(self):
    """Construct an an empty instance."""
    self.groups = {}
    self.users = {}

  def AddAccountsFromDatabase(self, account_db_path):
    """Add accounts from the database at |account_db_path| to self.

    Overrides previously loaded accounts.

    Args:
      account_db_path: path to file containing an account database.
    """
    raw_db = json_lib.ParseJsonFileWithComments(account_db_path)
    json_lib.AssertIsInstance(raw_db, dict, 'accounts database')

    # We don't mandate that an accounts database specify either field.
    raw_db.setdefault(USERS_KEY, [])
    raw_db.setdefault(GROUPS_KEY, [])
    user_list = json_lib.PopValueOfType(raw_db, USERS_KEY, list,
                                        'list of users in accounts database')
    group_list = json_lib.PopValueOfType(raw_db, GROUPS_KEY, list,
                                         'list of groups in accounts database')

    # We do mandate that the database contain only fields we know about.
    if raw_db:
      raise ValueError('Accounts database include unknown fields: %r' %
                       raw_db.keys())

    for user in user_list:
      json_lib.AssertIsInstance(
          user, dict, 'user specification in accounts database')
      self._AddUser(user)

    for group in group_list:
      json_lib.AssertIsInstance(
          group, dict, 'group specification in accounts database')
      self._AddGroup(group)

  def _AddUser(self, user_spec):
    """Add a user to this account database based on |user_spec|.

    Args:
      user_spec: dict of information from an accounts database.
          This fragment is expected to have been parsed from
          developer supplied JSON and will be type checked.
    """
    # By default, user accounts are locked and cannot be logged into.
    user_spec.setdefault(USER_PASSWORD_KEY, u'!')
    # By default, users don't get a shell.
    user_spec.setdefault(USER_SHELL_KEY, u'/bin/false')
    # By default, users don't get a home directory.
    user_spec.setdefault(USER_HOME_KEY, u'/dev/null')
    # By default, users don't get a fixed UID.
    user_spec.setdefault(USER_FIXED_ID_KEY, False)
    # By default, users don't need a comment.
    user_spec.setdefault(USER_COMMENT_KEY, u'')
    # By default, users are not defunct.
    user_spec.setdefault(USER_DEFUNCT_KEY, False)

    name = json_lib.PopValueOfType(user_spec, USER_NAME_KEY, unicode,
                                   'username from user spec')
    password = json_lib.PopValueOfType(user_spec, USER_PASSWORD_KEY, unicode,
                                       'password for user %s' % name)
    uid = json_lib.PopValueOfType(user_spec, USER_ID_KEY, int,
                                  'default uid for user %s' % name)
    group_name = json_lib.PopValueOfType(user_spec, USER_GROUP_KEY, unicode,
                                         'primary group for user %s' % name)
    description = json_lib.PopValueOfType(user_spec, USER_COMMENT_KEY, unicode,
                                          'description for user %s' % name)
    home = json_lib.PopValueOfType(user_spec, USER_HOME_KEY, unicode,
                                   'home directory for user %s' % name)
    shell = json_lib.PopValueOfType(user_spec, USER_SHELL_KEY, unicode,
                                    'shell for user %s' % name)
    is_fixed_id = json_lib.PopValueOfType(user_spec, USER_FIXED_ID_KEY, bool,
                                          'whether UID for user %s is fixed' %
                                          name)
    is_defunct = json_lib.PopValueOfType(user_spec, USER_DEFUNCT_KEY, bool,
                                         'whether user %s is defunct.' % name)

    if user_spec:
      raise ValueError('Unexpected keys in user spec for user %s: %r' %
                       (name, user_spec.keys()))

    self.users[name] = User(name=name, password=password, uid=uid,
                            group_name=group_name, description=description,
                            home=home, shell=shell, is_fixed_id=is_fixed_id,
                            is_defunct=is_defunct)

  def _AddGroup(self, group_spec):
    """Add a group to this account database based on |group_spec|.

    Args:
      group_spec: dict of information from an accounts database.
          This fragment is expected to have been parsed from
          developer supplied JSON and will be type checked.
    """
    # By default, groups don't get a fixed GID.
    group_spec.setdefault(GROUP_FIXED_ID_KEY, False)
    # By default, groups don't get a password.
    group_spec.setdefault(GROUP_PASSWORD_KEY, u'!')
    # By default, groups are not defunct.
    group_spec.setdefault(GROUP_DEFUNCT_KEY, False)

    name = json_lib.PopValueOfType(group_spec, GROUP_NAME_KEY, unicode,
                                   'groupname from group spec')
    password = json_lib.PopValueOfType(group_spec, GROUP_PASSWORD_KEY, unicode,
                                       'password for group %s' % name)
    gid = json_lib.PopValueOfType(group_spec, GROUP_ID_KEY, int,
                                  'gid for group %s' % name)
    users = json_lib.PopValueOfType(group_spec, GROUP_USERS_KEY, list,
                                    'users in group %s' % name)
    is_fixed_id = json_lib.PopValueOfType(group_spec, GROUP_FIXED_ID_KEY, bool,
                                          'whether GID for group %s is fixed' %
                                          name)
    is_defunct = json_lib.PopValueOfType(group_spec, GROUP_DEFUNCT_KEY, bool,
                                         'whether group %s is defunct' % name)

    for username in users:
      json_lib.AssertIsInstance(username, unicode, 'user in group %s' % name)

    if group_spec:
      raise ValueError('Unexpected keys in group spec for group %s: %r' %
                       (name, group_spec.keys()))

    self.groups[name] = Group(name=name, password=password, gid=gid,
                              users=users, is_fixed_id=is_fixed_id,
                              is_defunct=is_defunct)
