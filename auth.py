# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Authentication related functions."""

import collections
import optparse


# Authentication configuration extracted from command line options.
# See doc string for 'make_auth_config' for meaning of fields.
AuthConfig = collections.namedtuple('AuthConfig', [
    'use_oauth2', # deprecated, will be always True
    'save_cookies', # deprecated, will be removed
    'use_local_webserver',
    'webserver_port',
])


def make_auth_config(
    use_oauth2=None,
    save_cookies=None,
    use_local_webserver=None,
    webserver_port=None):
  """Returns new instance of AuthConfig.

  If some config option is None, it will be set to a reasonable default value.
  This function also acts as an authoritative place for default values of
  corresponding command line options.
  """
  default = lambda val, d: val if val is not None else d
  return AuthConfig(
      default(use_oauth2, False),
      default(save_cookies, True),
      default(use_local_webserver, True),
      default(webserver_port, 8090))


def add_auth_options(parser):
  """Appends OAuth related options to OptionParser."""
  default_config = make_auth_config()
  parser.auth_group = optparse.OptionGroup(parser, 'Auth options')
  parser.auth_group.add_option(
      '--oauth2',
      action='store_true',
      dest='use_oauth2',
      default=default_config.use_oauth2,
      help='Use OAuth 2.0 instead of a password.')
  parser.auth_group.add_option(
      '--no-oauth2',
      action='store_false',
      dest='use_oauth2',
      default=default_config.use_oauth2,
      help='Use password instead of OAuth 2.0.')
  parser.auth_group.add_option(
      '--no-cookies',
      action='store_false',
      dest='save_cookies',
      default=default_config.save_cookies,
      help='Do not save authentication cookies to local disk.')
  parser.auth_group.add_option(
      '--auth-no-local-webserver',
      action='store_false',
      dest='use_local_webserver',
      default=default_config.use_local_webserver,
      help='Do not run a local web server when performing OAuth2 login flow.')
  parser.auth_group.add_option(
      '--auth-host-port',
      type=int,
      default=default_config.webserver_port,
      help='Port a local web server should listen on. Used only if '
          '--auth-no-local-webserver is not set. [default: %default]')
  parser.add_option_group(parser.auth_group)


def extract_auth_config_from_options(options):
  """Given OptionParser parsed options, extracts AuthConfig from it.

  OptionParser should be populated with auth options by 'add_auth_options'.
  """
  return make_auth_config(
      use_oauth2=options.use_oauth2,
      save_cookies=False if options.use_oauth2 else options.save_cookies,
      use_local_webserver=options.use_local_webserver,
      webserver_port=options.auth_host_port)


def auth_config_to_command_options(auth_config):
  """AuthConfig -> list of strings with command line options."""
  if not auth_config:
    return []
  opts = ['--oauth2' if auth_config.use_oauth2 else '--no-oauth2']
  if not auth_config.save_cookies:
    opts.append('--no-cookies')
  if not auth_config.use_local_webserver:
    opts.append('--auth-no-local-webserver')
  opts.extend(['--auth-host-port', str(auth_config.webserver_port)])
  return opts
