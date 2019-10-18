# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Google OAuth2 related functions."""

from __future__ import print_function

import collections
import datetime
import functools
import json
import logging
import optparse
import os
import sys
import threading
import urllib
import urlparse

import subprocess2

from third_party import httplib2


# depot_tools/.
DEPOT_TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))

# This is what most GAE apps require for authentication.
OAUTH_SCOPE_EMAIL = 'https://www.googleapis.com/auth/userinfo.email'
# Gerrit and Git on *.googlesource.com require this scope.
OAUTH_SCOPE_GERRIT = 'https://www.googleapis.com/auth/gerritcodereview'
# Deprecated. Use OAUTH_SCOPE_EMAIL instead.
OAUTH_SCOPES = OAUTH_SCOPE_EMAIL


# Mockable datetime.datetime.utcnow for testing.
def datetime_now():
  return datetime.datetime.utcnow()


# Authentication configuration extracted from command line options.
# See doc string for 'make_auth_config' for meaning of fields.
AuthConfig = collections.namedtuple('AuthConfig', [
    'use_oauth2', # deprecated, will be always True
    'save_cookies', # deprecated, will be removed
    'use_local_webserver',
    'webserver_port',
])


# OAuth access token with its expiration time (UTC datetime or None if unknown).
class AccessToken(collections.namedtuple('AccessToken', [
    'token',
    'expires_at',
  ])):

  def needs_refresh(self, now=None):
    """True if this AccessToken should be refreshed."""
    if self.expires_at is not None:
      now = now or datetime_now()
      # Allow 30s of clock skew between client and backend.
      now += datetime.timedelta(seconds=30)
      return now >= self.expires_at
    # Token without expiration time never expires.
    return False


class AuthenticationError(Exception):
  """Raised on errors related to authentication."""


class LoginRequiredError(AuthenticationError):
  """Interaction with the user is required to authenticate."""

  def __init__(self, scopes=OAUTH_SCOPE_EMAIL):
    msg = (
        'You are not logged in. Please login first by running:\n'
        '  luci-auth login -scopes %s' % scopes)
    super(LoginRequiredError, self).__init__(msg)


class LuciContextAuthError(Exception):
  """Raised on errors related to unsuccessful attempts to load LUCI_CONTEXT"""

  def __init__(self, msg, exc=None):
    if exc is None:
      logging.error(msg)
    else:
      logging.exception(msg)
      msg = '%s: %s' % (msg, exc)
    super(LuciContextAuthError, self).__init__(msg)


def has_luci_context_local_auth():
  """Returns whether LUCI_CONTEXT should be used for ambient authentication.
  """
  try:
    params = _get_luci_context_local_auth_params()
  except LuciContextAuthError:
    return False
  if params is None:
    return False
  return bool(params.default_account_id)


# TODO(crbug.com/1001756): Remove. luci-auth uses local auth if available,
# making this unnecessary.
def get_luci_context_access_token(scopes=OAUTH_SCOPE_EMAIL):
  """Returns a valid AccessToken from the local LUCI context auth server.

  Adapted from
  https://chromium.googlesource.com/infra/luci/luci-py/+/master/client/libs/luci_context/luci_context.py
  See the link above for more details.

  Returns:
    AccessToken if LUCI_CONTEXT is present and attempt to load it is successful.
    None if LUCI_CONTEXT is absent.

  Raises:
    LuciContextAuthError if LUCI_CONTEXT is present, but there was a failure
    obtaining its access token.
  """
  params = _get_luci_context_local_auth_params()
  if params is None:
    return None
  return _get_luci_context_access_token(
      params, datetime.datetime.utcnow(), scopes)


_LuciContextLocalAuthParams = collections.namedtuple(
  '_LuciContextLocalAuthParams', [
    'default_account_id',
    'secret',
    'rpc_port',
])


def _cache_thread_safe(f):
  """Decorator caching result of nullary function in thread-safe way."""
  lock = threading.Lock()
  cache = []

  @functools.wraps(f)
  def caching_wrapper():
    if not cache:
      with lock:
        if not cache:
          cache.append(f())
    return cache[0]

  # Allow easy way to clear cache, particularly useful in tests.
  caching_wrapper.clear_cache = lambda: cache.pop() if cache else None
  return caching_wrapper


@_cache_thread_safe
def _get_luci_context_local_auth_params():
  """Returns local auth parameters if local auth is configured else None.

  Raises LuciContextAuthError on unexpected failures.
  """
  ctx_path = os.environ.get('LUCI_CONTEXT')
  if not ctx_path:
    return None
  ctx_path = ctx_path.decode(sys.getfilesystemencoding())
  try:
    loaded = _load_luci_context(ctx_path)
  except (OSError, IOError, ValueError) as e:
    raise LuciContextAuthError('Failed to open, read or decode LUCI_CONTEXT', e)
  try:
    local_auth = loaded.get('local_auth')
  except AttributeError as e:
    raise LuciContextAuthError('LUCI_CONTEXT not in proper format', e)
  if local_auth is None:
    logging.debug('LUCI_CONTEXT configured w/o local auth')
    return None
  try:
    return _LuciContextLocalAuthParams(
        default_account_id=local_auth.get('default_account_id'),
        secret=local_auth.get('secret'),
        rpc_port=int(local_auth.get('rpc_port')))
  except (AttributeError, ValueError) as e:
    raise LuciContextAuthError('local_auth config malformed', e)


def _load_luci_context(ctx_path):
  # Kept separate for test mocking.
  with open(ctx_path) as f:
    return json.load(f)


def _get_luci_context_access_token(params, now, scopes=OAUTH_SCOPE_EMAIL):
  # No account, local_auth shouldn't be used.
  if not params.default_account_id:
    return None
  if not params.secret:
    raise LuciContextAuthError('local_auth: no secret')

  logging.debug('local_auth: requesting an access token for account "%s"',
      params.default_account_id)
  http = httplib2.Http()
  host = '127.0.0.1:%d' % params.rpc_port
  resp, content = http.request(
      uri='http://%s/rpc/LuciLocalAuthService.GetOAuthToken' % host,
      method='POST',
      body=json.dumps({
        'account_id': params.default_account_id,
        'scopes': scopes.split(' '),
        'secret': params.secret,
      }),
      headers={'Content-Type': 'application/json'})
  if resp.status != 200:
    raise LuciContextAuthError(
        'local_auth: Failed to grab access token from '
        'LUCI context server with status %d: %r' % (resp.status, content))
  try:
    token = json.loads(content)
    error_code = token.get('error_code')
    error_message = token.get('error_message')
    access_token = token.get('access_token')
    expiry = token.get('expiry')
  except (AttributeError, ValueError) as e:
    raise LuciContextAuthError('Unexpected access token response format', e)
  if error_code:
    raise LuciContextAuthError(
        'Error %d in retrieving access token: %s', error_code, error_message)
  if not access_token:
    raise LuciContextAuthError(
        'No access token returned from LUCI context server')
  expiry_dt = None
  if expiry:
    try:
      expiry_dt = datetime.datetime.utcfromtimestamp(expiry)
      logging.debug(
        'local_auth: got an access token for '
        'account "%s" that expires in %d sec',
        params.default_account_id, (expiry_dt - now).total_seconds())
    except (TypeError, ValueError) as e:
      raise LuciContextAuthError('Invalid expiry in returned token', e)
  else:
    logging.debug(
        'local auth: got an access token for account "%s" that does not expire',
        params.default_account_id)
  access_token = AccessToken(access_token, expiry_dt)
  if access_token.needs_refresh(now=now):
    raise LuciContextAuthError('Received access token is already expired')
  return access_token


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
      default(use_oauth2, True),
      default(save_cookies, True),
      default(use_local_webserver, not _is_headless()),
      default(webserver_port, 8090))


def add_auth_options(parser, default_config=None):
  """Appends OAuth related options to OptionParser."""
  default_config = default_config or make_auth_config()
  parser.auth_group = optparse.OptionGroup(parser, 'Auth options')
  parser.add_option_group(parser.auth_group)

  # OAuth2 vs password switch.
  auth_default = 'use OAuth2' if default_config.use_oauth2 else 'use password'
  parser.auth_group.add_option(
      '--oauth2',
      action='store_true',
      dest='use_oauth2',
      default=default_config.use_oauth2,
      help='Use OAuth 2.0 instead of a password. [default: %s]' % auth_default)
  parser.auth_group.add_option(
      '--no-oauth2',
      action='store_false',
      dest='use_oauth2',
      default=default_config.use_oauth2,
      help='Use password instead of OAuth 2.0. [default: %s]' % auth_default)

  # Password related options, deprecated.
  parser.auth_group.add_option(
      '--no-cookies',
      action='store_false',
      dest='save_cookies',
      default=default_config.save_cookies,
      help='Do not save authentication cookies to local disk.')

  # OAuth2 related options.
  # TODO(crbug.com/1001756): Remove. No longer supported.
  parser.auth_group.add_option(
      '--auth-no-local-webserver',
      action='store_false',
      dest='use_local_webserver',
      default=default_config.use_local_webserver,
      help='DEPRECATED. Do not use')
  parser.auth_group.add_option(
      '--auth-host-port',
      type=int,
      default=default_config.webserver_port,
      help='DEPRECATED. Do not use')
  parser.auth_group.add_option(
      '--auth-refresh-token-json',
      help='DEPRECATED. Do not use')


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
  """AuthConfig -> list of strings with command line options.

  Omits options that are set to default values.
  """
  if not auth_config:
    return []
  defaults = make_auth_config()
  opts = []
  if auth_config.use_oauth2 != defaults.use_oauth2:
    opts.append('--oauth2' if auth_config.use_oauth2 else '--no-oauth2')
  if auth_config.save_cookies != auth_config.save_cookies:
    if not auth_config.save_cookies:
      opts.append('--no-cookies')
  if auth_config.use_local_webserver != defaults.use_local_webserver:
    if not auth_config.use_local_webserver:
      opts.append('--auth-no-local-webserver')
  if auth_config.webserver_port != defaults.webserver_port:
    opts.extend(['--auth-host-port', str(auth_config.webserver_port)])
  return opts


def get_authenticator(config, scopes=OAUTH_SCOPE_EMAIL):
  """Returns Authenticator instance to access given host.

  Args:
    config: AuthConfig instance.
    scopes: space separated oauth scopes. Defaults to OAUTH_SCOPE_EMAIL.

  Returns:
    Authenticator object.
  """
  return Authenticator(config, scopes)


class Authenticator(object):
  """Object that knows how to refresh access tokens when needed.

  Args:
    config: AuthConfig object that holds authentication configuration.
  """

  def __init__(self, config, scopes):
    assert isinstance(config, AuthConfig)
    assert config.use_oauth2
    self._access_token = None
    self._config = config
    self._lock = threading.Lock()
    self._scopes = scopes
    logging.debug('Using auth config %r', config)

  def has_cached_credentials(self):
    """Returns True if credentials can be obtained.

    If returns False, get_access_token() later will probably ask for interactive
    login by raising LoginRequiredError, unless local auth is configured.

    If returns True, get_access_token() won't ask for interactive login.
    """
    with self._lock:
      return bool(self._get_luci_auth_token())

  def get_access_token(self, force_refresh=False, allow_user_interaction=False,
                       use_local_auth=True):
    """Returns AccessToken, refreshing it if necessary.

    Args:
      TODO(crbug.com/1001756): Remove.
      force_refresh: Ignored, luci-auth doesn't support force-refreshing tokens.
      allow_user_interaction: Ignored. allow_user_interaction is always False.
      use_local_auth: Ignored. luci-auth already covers local_auth.

    Raises:
      AuthenticationError on error or if authentication flow was interrupted.
      LoginRequiredError if user interaction is required, but
          allow_user_interaction is False.
    """
    with self._lock:
      if self._access_token and not self._access_token.needs_refresh():
        return self._access_token

      # Token expired or missing. Maybe some other process already updated it,
      # reload from the cache.
      self._access_token = self._get_luci_auth_token()
      if self._access_token and not self._access_token.needs_refresh():
        return self._access_token

      # Nope, still expired, need to run the refresh flow.
      logging.error('Failed to create access token')
      raise LoginRequiredError(self._scopes)

  def authorize(self, http):
    """Monkey patches authentication logic of httplib2.Http instance.

    The modified http.request method will add authentication headers to each
    request.

    Args:
       http: An instance of httplib2.Http.

    Returns:
       A modified instance of http that was passed in.
    """
    # Adapted from oauth2client.OAuth2Credentials.authorize.
    request_orig = http.request

    @functools.wraps(request_orig)
    def new_request(
        uri, method='GET', body=None, headers=None,
        redirections=httplib2.DEFAULT_MAX_REDIRECTS,
        connection_type=None):
      headers = (headers or {}).copy()
      headers['Authorization'] = 'Bearer %s' % self.get_access_token().token
      return request_orig(
          uri, method, body, headers, redirections, connection_type)

    http.request = new_request
    return http

  ## Private methods.

  def _run_luci_auth_login(self):
    """Run luci-auth login.

    Returns:
      AccessToken with credentials.
    """
    logging.debug('Running luci-auth login')
    subprocess2.check_call(['luci-auth', 'login', '-scopes', self._scopes])
    return self._get_luci_auth_token()

  def _get_luci_auth_token(self):
    logging.debug('Running luci-auth token')
    try:
      out, err = subprocess2.check_call_out(
          ['luci-auth', 'token', '-scopes', self._scopes, '-json-output', '-'],
          stdout=subprocess2.PIPE, stderr=subprocess2.PIPE)
      logging.debug('luci-auth token stderr:\n%s', err)
      token_info = json.loads(out)
      return AccessToken(
          token_info['token'],
          datetime.datetime.utcfromtimestamp(token_info['expiry']))
    except subprocess2.CalledProcessError:
      return None


## Private functions.


def _is_headless():
  """True if machine doesn't seem to have a display."""
  return sys.platform == 'linux2' and not os.environ.get('DISPLAY')
