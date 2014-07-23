#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Client tool to perform various authentication related tasks."""

__version__ = '0.3.1'

import optparse
import sys

from third_party import colorama
from third_party.depot_tools import fix_encoding
from third_party.depot_tools import subcommand

from utils import on_error
from utils import net
from utils import oauth
from utils import tools


class AuthServiceError(Exception):
  """Unexpected response from authentication service."""


class AuthService(object):
  """Represents remote Authentication service."""

  def __init__(self, url):
    self._service = net.get_http_service(url)

  def login(self, allow_user_interaction):
    """Refreshes cached access token or creates a new one."""
    return self._service.login(allow_user_interaction)

  def logout(self):
    """Purges cached access token."""
    return self._service.logout()

  def get_current_identity(self):
    """Returns identity associated with currently used credentials.

    Identity is a string:
      user:<email> - if using OAuth or cookie based authentication.
      bot:<id> - if using HMAC based authentication.
      anonymous:anonymous - if not authenticated.
    """
    identity = self._service.json_request('GET', '/auth/api/v1/accounts/self')
    if not identity:
      raise AuthServiceError('Failed to fetch identity')
    return identity['identity']


def add_auth_options(parser):
  """Adds command line options related to authentication."""
  parser.auth_group = optparse.OptionGroup(parser, 'Authentication')
  parser.auth_group.add_option(
      '--auth-method',
      metavar='METHOD',
      default=net.get_default_auth_config()[0],
      help='Authentication method to use: %s. [default: %%default]' %
          ', '.join(name for name, _ in net.AUTH_METHODS))
  parser.add_option_group(parser.auth_group)
  oauth.add_oauth_options(parser)


def process_auth_options(parser, options):
  """Configures process-wide authentication parameters based on |options|."""
  # Validate that authentication method is known.
  if options.auth_method not in dict(net.AUTH_METHODS):
    parser.error('Invalid --auth-method value: %s' % options.auth_method)

  # Process the rest of the flags based on actual method used.
  # Only oauth is configurable now.
  config = None
  if options.auth_method == 'oauth':
    config = oauth.extract_oauth_config_from_options(options)

  # Now configure 'net' globally to use this for every request.
  net.configure_auth(options.auth_method, config)


def ensure_logged_in(server_url):
  """Checks that user is logged in, asking to do it if not.

  Aborts the process with exit code 1 if user is not logged it. Noop when used
  on bots.
  """
  if net.get_auth_method() not in ('cookie', 'oauth'):
    return
  server_url = server_url.lower().rstrip('/')
  assert server_url.startswith(('https://', 'http://localhost:')), server_url
  service = AuthService(server_url)
  service.login(False)
  identity = service.get_current_identity()
  if identity == 'anonymous:anonymous':
    print >> sys.stderr, (
        'Please login to %s: \n'
        '  python auth.py login --service=%s' % (server_url, server_url))
    sys.exit(1)
  email = identity.split(':')[1]
  print >> sys.stderr, 'Logged in to %s: %s' % (server_url, email)


@subcommand.usage('[options]')
def CMDlogin(parser, args):
  """Runs interactive login flow and stores auth token/cookie on disk."""
  (options, args) = parser.parse_args(args)
  process_auth_options(parser, options)
  service = AuthService(options.service)
  if service.login(True):
    print 'Logged in as \'%s\'.' % service.get_current_identity()
    return 0
  else:
    print 'Login failed or canceled.'
    return 1


@subcommand.usage('[options]')
def CMDlogout(parser, args):
  """Purges cached auth token/cookie."""
  (options, args) = parser.parse_args(args)
  process_auth_options(parser, options)
  service = AuthService(options.service)
  service.logout()
  return 0


@subcommand.usage('[options]')
def CMDcheck(parser, args):
  """Shows identity associated with currently cached auth token/cookie."""
  (options, args) = parser.parse_args(args)
  process_auth_options(parser, options)
  service = AuthService(options.service)
  service.login(False)
  print service.get_current_identity()
  return 0


class OptionParserAuth(tools.OptionParserWithLogging):
  def __init__(self, **kwargs):
    tools.OptionParserWithLogging.__init__(self, prog='auth.py', **kwargs)
    self.server_group = tools.optparse.OptionGroup(self, 'Server')
    self.server_group.add_option(
        '-S', '--service',
        metavar='URL', default='',
        help='Service to use')
    self.add_option_group(self.server_group)
    add_auth_options(self)

  def parse_args(self, *args, **kwargs):
    options, args = tools.OptionParserWithLogging.parse_args(
        self, *args, **kwargs)
    options.service = options.service.rstrip('/')
    if not options.service:
      self.error('--service is required.')
    on_error.report_on_exception_exit(options.service)
    return options, args


def main(args):
  dispatcher = subcommand.CommandDispatcher(__name__)
  return dispatcher.execute(OptionParserAuth(version=__version__), args)


if __name__ == '__main__':
  fix_encoding.fix_encoding()
  tools.disable_buffering()
  colorama.init()
  sys.exit(main(sys.argv[1:]))
