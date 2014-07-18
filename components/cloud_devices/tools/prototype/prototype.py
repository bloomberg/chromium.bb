#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Prototype of cloud device with support of local API.

  This prototype has tons of flaws, not the least of which being that it
  occasionally will block while waiting for commands to finish. However, this is
  a quick sketch.
  Script requires following components:
    sudo apt-get install python-tornado
    sudo apt-get install python-pip
    sudo pip install google-api-python-client
    sudo pip install ecdsa
"""

import atexit
import base64
import datetime
import json
import os
import random
import subprocess
import time
import traceback

from apiclient.discovery import build_from_document
from apiclient.errors import HttpError
import httplib2
from oauth2client.client import AccessTokenRefreshError
from oauth2client.client import OAuth2WebServerFlow
from oauth2client.file import Storage
from tornado.httpserver import HTTPServer
from tornado.ioloop import IOLoop

_OAUTH_SCOPE = 'https://www.googleapis.com/auth/clouddevices'

_CONFIG_FILE = 'config.json'
_API_DISCOVERY_FILE = 'discovery.json'
_DEVICE_STATE_FILE = 'device_state.json'

_DEVICE_SETUP_SSID = 'GCD Prototype %02d..Bcamprv'
_DEVICE_NAME = 'GCD Prototype'
_DEVICE_TYPE = 'vendor'
_DEVICE_PORT = 8080

DEVICE_DRAFT = {
    'systemName': 'LEDFlasher',
    'deviceKind': 'vendor',
    'displayName': 'LED Flasher',
    'channel': {
        'supportedType': 'xmpp'
    },
    'commands': {
        'base': {
            'vendorCommands': [{
                'name': 'flashLED',
                'parameter': [{
                    'name': 'times',
                    'type': 'string'
                }]
            }]
        }
    }
}

wpa_supplicant_cmd = 'wpa_supplicant -Dwext -i%s -cwpa_supplicant.conf'
ifconfig_cmd = 'ifconfig %s 192.168.0.3'
hostapd_cmd = 'hostapd hostapd-min.conf'
dhclient_release = 'dhclient -r %s'
dhclient_renew = 'dhclient %s'
dhcpd_cmd = 'udhcpd -f udhcpd.conf'

wpa_supplicant_conf = 'wpa_supplicant.conf'

wpa_supplicant_template = """
network={
  ssid="%s"
  scan_ssid=1
  proto=WPA RSN
  key_mgmt=WPA-PSK
  pairwise=CCMP TKIP
  group=CCMP TKIP
  psk="%s"
}"""

hostapd_conf = 'hostapd-min.conf'

hostapd_template = """
interface=%s
driver=nl80211
ssid=%s
channel=1
"""

udhcpd_conf = 'udhcpd.conf'

udhcpd_template = """
start        192.168.0.20
end          192.168.0.254
interface    %s
"""

class DeviceUnregisteredError(Exception):
  pass


def ignore_errors(func):
  def inner(*args, **kwargs):
    try:
      func(*args, **kwargs)
    except Exception:  # pylint: disable=broad-except
      print 'Got error in unsafe function:'
      traceback.print_exc()
  return inner


class CommandWrapperReal(object):
  """Command wrapper that executs shell commands."""

  def __init__(self, cmd):
    if type(cmd) in [str, unicode]:
      cmd = cmd.split()
    self.cmd = cmd
    self.cmd_str = ' '.join(cmd)
    self.process = None

  def start(self):
    print 'Start: ', self.cmd_str
    if self.process:
      self.end()
    self.process = subprocess.Popen(self.cmd)

  def wait(self):
    print 'Wait: ', self.cmd_str
    self.process.wait()

  def end(self):
    print 'End: ', self.cmd_str
    if self.process:
      self.process.terminate()


class CommandWrapperFake(object):
  """Command wrapper that just prints shell commands."""

  def __init__(self, cmd):
    self.cmd_str = ' '.join(cmd)

  def start(self):
    print 'Fake start: ', self.cmd_str

  def wait(self):
    print 'Fake wait: ', self.cmd_str

  def end(self):
    print 'Fake end: ', self.cmd_str


class CloudCommandHandlerFake(object):
  """Prints devices commands without execution."""

  def __init__(self, ioloop):
    pass

  def handle_command(self, command_name, args):
    if command_name == 'flashLED':
      times = 1
      if 'times' in args:
        times = int(args['times'])
      print 'Flashing LED %d times' % times


class CloudCommandHandlerReal(object):
  """Executes device commands."""

  def __init__(self, ioloop, led_path):
    self.ioloop = ioloop
    self.led_path = led_path

  def handle_command(self, command_name, args):
    if command_name == 'flashLED':
      times = 1
      if 'times' in args:
        times = int(args['times'])
      print 'Really flashing LED %d times' % times
      self.flash_led(times)

  @ignore_errors
  def flash_led(self, times):
    self.set_led(times*2, True)

  def set_led(self, times, value):
    """Set led value."""
    if not times:
      return

    file_trigger = open(os.path.join(self.led_path, 'brightness'), 'w')

    if value:
      file_trigger.write('1')
    else:
      file_trigger.write('0')

    file_trigger.close()

    self.ioloop.add_timeout(datetime.timedelta(milliseconds=500),
                            lambda: self.set_led(times - 1, not value))


class WifiHandler(object):
  """Base class for wifi handlers."""

  class Delegate(object):

    def on_wifi_connected(self, unused_token):
      """Token is optional, and all delegates should support it being None."""
      raise Exception('Unhandled condition: WiFi connected')

  def __init__(self, ioloop, state, config, setup_ssid, delegate):
    self.ioloop = ioloop
    self.state = state
    self.delegate = delegate
    self.setup_ssid = setup_ssid
    self.interface = config['wireless_interface']

  def start(self):
    raise Exception('Start not implemented!')

  def get_ssid(self):
    raise Exception('Get SSID not implemented!')


class WifiHandlerReal(WifiHandler):
  """Real wifi handler.

     Note that by using CommandWrapperFake, you can run WifiHandlerReal on fake
     devices for testing the wifi-specific logic.
  """

  def __init__(self, ioloop, state, config, setup_ssid, delegate):
    super(WifiHandlerReal, self).__init__(ioloop, state, config,
                                          setup_ssid, delegate)

    if config['simulate_commands']:
      self.command_wrapper = CommandWrapperFake
    else:
      self.command_wrapper = CommandWrapperReal
    self.hostapd = self.command_wrapper(hostapd_cmd)
    self.wpa_supplicant = self.command_wrapper(
      wpa_supplicant_cmd % self.interface)
    self.dhcpd = self.command_wrapper(dhcpd_cmd)

  def start(self):
    if self.state.has_wifi():
      self.switch_to_wifi(self.state.ssid(), self.state.password(), None)
    else:
      self.start_hostapd()

  def start_hostapd(self):
    hostapd_config = open(hostapd_conf, 'w')
    hostapd_config.write(hostapd_template % (self.interface, self.setup_ssid))
    hostapd_config.close()

    self.hostapd.start()
    time.sleep(3)
    self.run_command(ifconfig_cmd % self.interface)
    self.dhcpd.start()

  def switch_to_wifi(self, ssid, passwd, token):
    try:
      udhcpd_config = open(udhcpd_conf, 'w')
      udhcpd_config.write(udhcpd_template % self.interface)
      udhcpd_config.close()

      wpa_config = open(wpa_supplicant_conf, 'w')
      wpa_config.write(wpa_supplicant_template % (ssid, passwd))
      wpa_config.close()

      self.hostapd.end()
      self.dhcpd.end()
      self.wpa_supplicant.start()
      self.run_command(dhclient_release % self.interface)
      self.run_command(dhclient_renew % self.interface)

      self.state.set_wifi(ssid, passwd)
      self.delegate.on_wifi_connected(token)
    except DeviceUnregisteredError:
      self.state.reset()
      self.wpa_supplicant.end()
      self.start_hostapd()

  def stop(self):
    self.hostapd.end()
    self.wpa_supplicant.end()
    self.dhcpd.end()

  def get_ssid(self):
    return self.state.get_ssid()

  def run_command(self, cmd):
    wrapper = self.command_wrapper(cmd)
    wrapper.start()
    wrapper.wait()


class WifiHandlerPassthrough(WifiHandler):
  """Passthrough wifi handler."""

  def __init__(self, ioloop, state, config, setup_ssid, delegate):
    super(WifiHandlerPassthrough, self).__init__(ioloop, state, config,
                                                 setup_ssid, delegate)

  def start(self):
    self.delegate.on_wifi_connected(None)

  def switch_to_wifi(self, unused_ssid, unused_passwd, unused_token):
    raise Exception('Should not be reached')

  def stop(self):
    pass

  def get_ssid(self):
    return 'dummy'


class State(object):
  """Device state."""

  def __init__(self):
    self.oauth_storage_ = Storage('oauth_creds')
    self.clear()

  def clear(self):
    self.credentials_ = None
    self.has_credentials_ = False
    self.has_wifi_ = False
    self.ssid_ = ''
    self.password_ = ''
    self.device_id_ = ''

  def reset(self):
    self.clear()
    self.dump()

  def dump(self):
    """Saves device state to file."""
    json_obj = {
        'has_credentials': self.has_credentials_,
        'has_wifi': self.has_wifi_,
        'ssid': self.ssid_,
        'password': self.password_,
        'device_id': self.device_id_
    }
    statefile = open(_DEVICE_STATE_FILE, 'w')
    json.dump(json_obj, statefile)
    statefile.close()

    if self.has_credentials_:
      self.oauth_storage_.put(self.credentials_)

  def load(self):
    if os.path.exists(_DEVICE_STATE_FILE):
      statefile = open(_DEVICE_STATE_FILE, 'r')
      json_obj = json.load(statefile)
      statefile.close()

      self.has_credentials_ = json_obj['has_credentials']
      self.has_wifi_ = json_obj['has_wifi']
      self.ssid_ = json_obj['ssid']
      self.password_ = json_obj['password']
      self.device_id_ = json_obj['device_id']

      if self.has_credentials_:
        self.credentials_ = self.oauth_storage_.get()

  def set_credentials(self, credentials, device_id):
    self.device_id_ = device_id
    self.credentials_ = credentials
    self.has_credentials_ = True
    self.dump()

  def set_wifi(self, ssid, password):
    self.ssid_ = ssid
    self.password_ = password
    self.has_wifi_ = True
    self.dump()

  def has_wifi(self):
    return self.has_wifi_

  def has_credentials(self):
    return self.has_credentials_

  def credentials(self):
    return self.credentials_

  def ssid(self):
    return self.ssid_

  def password(self):
    return self.password_

  def device_id(self):
    return self.device_id_


class Config(object):
  """Configuration parameters (should not change)"""
  def __init__(self):
    if not os.path.isfile(_CONFIG_FILE):
      config = {
          'oauth_client_id': '',
          'oauth_secret': '',
          'api_key': '',
          'wireless_interface': ''
      }
      config_f = open(_CONFIG_FILE + '.sample', 'w')
      config_f.write(json.dumps(credentials, sort_keys=True,
                                     indent=2, separators=(',', ': ')))
      config_f.close()
      raise Exception('Missing ' + _CONFIG_FILE)

    config_f = open(_CONFIG_FILE)
    config = json.load(config_f)
    config_f.close()

    self.config = config

  def __getitem__(self, item):
    if item in self.config:
      return self.config[item]
    return None

class MDnsWrapper(object):
  """Handles mDNS requests to device."""

  def __init__(self, command_wrapper):
    self.command_wrapper = command_wrapper
    self.avahi_wrapper = None
    self.setup_name = None
    self.device_id = ''
    self.started = False

  def start(self):
    self.started = True
    self.run_command()

  def get_command(self):
    """Return the command to run mDNS daemon."""
    cmd = [
        'avahi-publish',
        '-s', '--subtype=_%s._sub._privet._tcp' % _DEVICE_TYPE,
        _DEVICE_NAME, '_privet._tcp', '%s' % _DEVICE_PORT,
        'txtvers=3',
        'type=%s' % _DEVICE_TYPE,
        'ty=%s' % _DEVICE_NAME,
        'id=%s' % self.device_id
    ]
    if self.setup_name:
      cmd.append('setup_ssid=' + self.setup_name)
    return cmd

  def run_command(self):
    if self.avahi_wrapper:
      self.avahi_wrapper.end()
      self.avahi_wrapper.wait()

    self.avahi_wrapper = self.command_wrapper(self.get_command())
    self.avahi_wrapper.start()

  def set_id(self, device_id):
    self.device_id = device_id
    if self.started:
      self.run_command()

  def set_setup_name(self, setup_name):
    self.setup_name = setup_name
    if self.started:
      self.run_command()


class CloudDevice(object):
  """Handles device registration and commands."""

  class Delegate(object):

    def on_device_started(self):
      raise Exception('Not implemented: Device started')

    def on_device_stopped(self):
      raise Exception('Not implemented: Device stopped')

  def __init__(self, ioloop, state, config, command_wrapper, delegate):
    self.state = state
    self.http = httplib2.Http()

    self.oauth_client_id = config['oauth_client_id']
    self.oauth_secret = config['oauth_secret']
    self.api_key = config['api_key']

    if not os.path.isfile(_API_DISCOVERY_FILE):
      raise Exception('Download https://developers.google.com/'
                      'cloud-devices/v1/discovery.json')

    f = open(_API_DISCOVERY_FILE)
    discovery = f.read()
    f.close()
    self.gcd = build_from_document(discovery, developerKey=self.api_key,
                                   http=self.http)

    self.ioloop = ioloop
    self.active = True
    self.device_id = None
    self.credentials = None
    self.delegate = delegate
    self.command_handler = command_wrapper

  def try_start(self, token):
    """Tries start or register device."""
    if self.state.has_credentials():
      self.credentials = self.state.credentials()
      self.device_id = self.state.device_id()
      self.run_device()
    elif token:
      self.register(token)
    else:
      print 'Device not registered and has no credentials.'
      print 'Waiting for registration.'

  def register(self, token):
    """Register device."""
    resource = {
        'deviceDraft': DEVICE_DRAFT,
        'oauthClientId': self.oauth_client_id
    }

    self.gcd.registrationTickets().patch(registrationTicketId=token,
                                         body=resource).execute()

    final_ticket = self.gcd.registrationTickets().finalize(
        registrationTicketId=token).execute()

    authorization_code = final_ticket['robotAccountAuthorizationCode']
    flow = OAuth2WebServerFlow(self.oauth_client_id, self.oauth_secret,
                               _OAUTH_SCOPE, redirect_uri='oob')
    self.credentials = flow.step2_exchange(authorization_code)
    self.device_id = final_ticket['deviceDraft']['id']
    self.state.set_credentials(self.credentials, self.device_id)
    print 'Registered with device_id ', self.device_id

    self.run_device()

  def run_device(self):
    """Runs device."""
    self.credentials.authorize(self.http)

    try:
      self.gcd.devices().get(deviceId=self.device_id).execute()
    except HttpError, e:
      # Pretty good indication the device was deleted
      if e.resp.status == 404:
        raise DeviceUnregisteredError()
    except AccessTokenRefreshError:
      raise DeviceUnregisteredError()

    self.check_commands()
    self.delegate.on_device_started()

  def check_commands(self):
    """Checks device commands."""
    if not self.active:
      return
    print 'Checking commands...'
    commands = self.gcd.commands().list(deviceId=self.device_id,
                                        state='queued').execute()

    if 'commands' in commands:
      print 'Found ', len(commands['commands']), ' commands'
      vendor_command_name = None

      for command in commands['commands']:
        try:
          if command['name'].startswith('base._'):
            vendor_command_name = command['name'][len('base._'):]
            if 'parameters' in command:
              parameters = command['parameters']
            else:
              parameters = {}
          else:
            vendor_command_name = None
        except KeyError:
          print 'Could not parse vendor command ',
          print repr(command)
          vendor_command_name = None

        if vendor_command_name:
          self.command_handler.handle_command(vendor_command_name, parameters)

        self.gcd.commands().patch(commandId=command['id'],
                                  body={'state': 'done'}).execute()
    else:
      print 'Found no commands'

    self.ioloop.add_timeout(datetime.timedelta(milliseconds=1000),
                            self.check_commands)

  def stop(self):
    self.active = False

  def get_device_id(self):
    return self.device_id


def get_only(f):
  def inner(self, request, response_func, *args):
    if request.method != 'GET':
      return False
    return f(self, request, response_func, *args)
  return inner


def post_only(f):
  def inner(self, request, response_func, *args):
    # if request.method != 'POST':
      # return False
    return f(self, request, response_func, *args)
  return inner


def wifi_provisioning(f):
  def inner(self, request, response_func, *args):
    if self.on_wifi:
      return False
    return f(self, request, response_func, *args)
  return inner


def post_provisioning(f):
  def inner(self, request, response_func, *args):
    if not self.on_wifi:
      return False
    return f(self, request, response_func, *args)
  return inner


class WebRequestHandler(WifiHandler.Delegate, CloudDevice.Delegate):
  """Handles HTTP requests."""

  class InvalidStepError(Exception):
    pass

  class InvalidPackageError(Exception):
    pass

  class EncryptionError(Exception):
    pass

  class CancelableClosure(object):
    """Allows to cancel callbacks."""

    def __init__(self, function):
      self.function = function

    def __call__(self):
      if self.function:
        return self.function
      return None

    def cancel(self):
      self.function = None

  class DummySession(object):
    """Handles sessions."""

    def __init__(self, session_id):
      self.session_id = session_id
      self.key = None

    def do_step(self, step, package):
      if step != 0:
        raise self.InvalidStepError()
      self.key = package
      return self.key

    def decrypt(self, cyphertext):
      return json.loads(cyphertext[len(self.key):])

    def encrypt(self, plain_data):
      return self.key + json.dumps(plain_data)

    def get_session_id(self):
      return self.session_id

    def get_stype(self):
      return 'dummy'

    def get_status(self):
      return 'complete'

  class EmptySession(object):
    """Handles sessions."""

    def __init__(self, session_id):
      self.session_id = session_id
      self.key = None

    def do_step(self, step, package):
      if step != 0 or package != '':
        raise self.InvalidStepError()
      return ''

    def decrypt(self, cyphertext):
      return json.loads(cyphertext)

    def encrypt(self, plain_data):
      return json.dumps(plain_data)

    def get_session_id(self):
      return self.session_id

    def get_stype(self):
      return 'empty'

    def get_status(self):
      return 'complete'

  def __init__(self, ioloop, state):
    self.config = Config()

    if self.config['on_real_device']:
      mdns_wrappers = CommandWrapperReal
      wifi_handler = WifiHandlerReal
    else:
      mdns_wrappers = CommandWrapperReal
      wifi_handler = WifiHandlerPassthrough


    if self.config['led_path']:
      cloud_wrapper = CloudCommandHandlerReal(ioloop,
                                                      self.config['led_path'])
      self.setup_real(self.config['led_path'])
    else:
      cloud_wrapper = CloudCommandHandlerFake(ioloop)
      self.setup_fake()

    self.setup_ssid = _DEVICE_SETUP_SSID % random.randint(0,99)
    self.cloud_device = CloudDevice(ioloop, state, self.config,
                                    cloud_wrapper, self)
    self.wifi_handler = wifi_handler(ioloop, state, self.config,
                                     self.setup_ssid, self)
    self.mdns_wrapper = MDnsWrapper(mdns_wrappers)
    self.on_wifi = False
    self.registered = False
    self.in_session = False
    self.ioloop = ioloop

    self.handlers = {
        '/internal/ping': self.do_ping,
        '/privet/info': self.do_info,
        '/deprecated/wifi/switch': self.do_wifi_switch,
        '/privet/v3/session/handshake': self.do_session_handshake,
        '/privet/v3/session/cancel': self.do_session_cancel,
        '/privet/v3/session/request': self.do_session_call,
        '/privet/v3/setup/start':
            self.get_insecure_api_handler(self.do_secure_setup_start),
        '/privet/v3/setup/cancel':
            self.get_insecure_api_handler(self.do_secure_setup_cancel),
        '/privet/v3/setup/status':
            self.get_insecure_api_handler(self.do_secure_status),
    }

    self.current_session = None
    self.session_cancel_callback = None
    self.session_handlers = {
        'dummy': self.DummySession,
        'empty': self.EmptySession
    }

    self.secure_handlers = {
        '/privet/v3/setup/start': self.do_secure_setup_start,
        '/privet/v3/setup/cancel': self.do_secure_setup_cancel,
        '/privet/v3/setup/status': self.do_secure_status
    }

  @staticmethod
  def setup_fake():
    print 'Skipping device setup'

  @staticmethod
  def setup_real(led_path):
    file_trigger = open(os.path.join(led_path, 'trigger'), 'w')
    file_trigger.write('none')
    file_trigger.close()

  def start(self):
    self.wifi_handler.start()
    self.mdns_wrapper.set_setup_name(self.setup_ssid)
    self.mdns_wrapper.start()

  @get_only
  def do_ping(self, unused_request, response_func):
    response_func(200, {'pong': True})
    return True

  @get_only
  def do_public_info(self, unused_request, response_func):
    info = dict(self.get_common_info().items() + {
        'stype': self.session_handlers.keys()}.items())
    response_func(200, info)

  @post_provisioning
  @get_only
  def do_info(self, unused_request, response_func):
    specific_info = {
        'x-privet-token': 'sample',
        'api': sorted(self.handlers.keys())
    }
    info = dict(self.get_common_info().items() + specific_info.items())
    response_func(200, info)
    return True

  @post_only
  @wifi_provisioning
  def do_wifi_switch(self, request, response_func):
    """Handles /deprecated/wifi/switch requests."""
    data = json.loads(request.body)
    try:
      ssid = data['ssid']
      passw = data['passw']
    except KeyError:
      print 'Malformed content: ' + repr(data)
      response_func(400, {'error': 'invalidParams'})
      traceback.print_exc()
      return True

    response_func(200, {'ssid': ssid})
    self.wifi_handler.switch_to_wifi(ssid, passw, None)
    # TODO(noamsml): Return to normal wifi after timeout (cancelable)
    return True

  @post_only
  def do_session_handshake(self, request, response_func):
    """Handles /privet/v3/session/handshake requests."""

    data = json.loads(request.body)
    try:
      stype = data['keyExchangeType']
      step = data['step']
      package = base64.b64decode(data['package'])
      if 'sessionID' in data:
        session_id = data['sessionID']
      else:
        session_id = "dummy"
    except (KeyError, TypeError):
      traceback.print_exc()
      print 'Malformed content: ' + repr(data)
      response_func(400, {'error': 'invalidParams'})
      return True

    if self.current_session:
      if session_id != self.current_session.get_session_id():
        response_func(400, {'error': 'maxSessionsExceeded'})
        return True
      if stype != self.current_session.get_stype():
        response_func(400, {'error': 'unsupportedKeyExchangeType'})
        return True
    else:
      if stype not in self.session_handlers:
        response_func(400, {'error': 'unsupportedKeyExchangeType'})
        return True
      self.current_session = self.session_handlers[stype](session_id)

    try:
      output_package = self.current_session.do_step(step, package)
    except self.InvalidStepError:
      response_func(400, {'error': 'invalidStep'})
      return True
    except self.InvalidPackageError:
      response_func(400, {'error': 'invalidPackage'})
      return True

    return_obj = {
        'status': self.current_session.get_status(),
        'step': step,
        'package': base64.b64encode(output_package),
        'sessionID': session_id
    }
    response_func(200, return_obj)
    self.post_session_cancel()
    return True

  @post_only
  def do_session_cancel(self, request, response_func):
    """Handles /privet/v3/session/cancel requests."""
    data = json.loads(request.body)
    try:
      session_id = data['sessionID']
    except KeyError:
      response_func(400, {'error': 'invalidParams'})
      return True

    if self.current_session and session_id == self.current_session.session_id:
      self.current_session = None
      if self.session_cancel_callback:
        self.session_cancel_callback.cancel()
      response_func(200, {'status': 'cancelled', 'sessionID': session_id})
    else:
      response_func(400, {'error': 'unknownSession'})
    return True

  @post_only
  def do_session_call(self, request, response_func):
    """Handles /privet/v3/session/call requests."""
    try:
      session_id = request.headers['X-Privet-SessionID']
    except KeyError:
      response_func(400, {'error': 'unknownSession'})
      return True

    if (not self.current_session or
        session_id != self.current_session.session_id):
      response_func(400, {'error': 'unknownSession'})
      return True

    try:
      decrypted = self.current_session.decrypt(request.body)
    except self.EncryptionError:
      response_func(400, {'error': 'encryptionError'})
      return True

    def encrypted_response_func(code, data):
      if 'error' in data:
        self.encrypted_send_response(request, code, dict(data.items() + {
            'api': decrypted['api']
        }.items()))
      else:
        self.encrypted_send_response(request, code, {
            'api': decrypted['api'],
            'output': data
        })

    if ('api' not in decrypted or 'input' not in decrypted or
        type(decrypted['input']) != dict):
      print 'Invalid params in API stage'
      encrypted_response_func(200, {'error': 'invalidParams'})
      return True

    if decrypted['api'] in self.secure_handlers:
      self.secure_handlers[decrypted['api']](request,
                                             encrypted_response_func,
                                             decrypted['input'])
    else:
      encrypted_response_func(200, {'error': 'unknownApi'})

    self.post_session_cancel()
    return True

  def get_insecure_api_handler(self, handler):
    def inner(request, func):
      return self.insecure_api_handler(request, func, handler)
    return inner

  @post_only
  def insecure_api_handler(self, request, response_func, handler):
    real_params = json.loads(request.body) if request.body else {}
    handler(request, response_func, real_params)
    return True

  def do_secure_status(self, unused_request, response_func, unused_params):
    """Handles /privet/v3/setup/status requests."""
    setup = {
        'registration': {
            'required': True
        },
        'wifi': {
            'required': True
        }
    }
    if self.on_wifi:
      setup['wifi']['status'] = 'complete'
      setup['wifi']['ssid'] = ''  # TODO(noamsml): Add SSID to status
    else:
      setup['wifi']['status'] = 'available'

    if self.cloud_device.get_device_id():
      setup['registration']['status'] = 'complete'
      setup['registration']['id'] = self.cloud_device.get_device_id()
    else:
      setup['registration']['status'] = 'available'
    response_func(200, setup)

  def do_secure_setup_start(self, unused_request, response_func, params):
    """Handles /privet/v3/setup/start requests."""
    has_wifi = False
    token = None

    try:
      if 'wifi' in params:
        has_wifi = True
        ssid = params['wifi']['ssid']
        passw = params['wifi']['passphrase']

      if 'registration' in params:
        token = params['registration']['ticketID']
    except KeyError:
      print 'Invalid params in bootstrap stage'
      response_func(400, {'error': 'invalidParams'})
      return

    try:
      if has_wifi:
        self.wifi_handler.switch_to_wifi(ssid, passw, token)
      elif token:
        self.cloud_device.register(token)
      else:
        response_func(400, {'error': 'invalidParams'})
        return
    except HttpError:
      pass  # TODO(noamsml): store error message in this case

    self.do_secure_status(unused_request, response_func, params)

  def do_secure_setup_cancel(self, request, response_func, params):
    pass

  def handle_request(self, request):
    def response_func(code, data):
      self.real_send_response(request, code, data)

    handled = False
    print '[INFO] %s %s' % (request.method, request.path)
    if request.path in self.handlers:
      handled = self.handlers[request.path](request, response_func)

    if not handled:
      self.real_send_response(request, 404, {'error': 'notFound'})

  def encrypted_send_response(self, request, code, data):
    self.raw_send_response(request, code,
                           self.current_session.encrypt(data))

  def real_send_response(self, request, code, data):
    data = json.dumps(data, sort_keys=True, indent=2, separators=(',', ': '))
    data += '\n'
    self.raw_send_response(request, code, data)

  def raw_send_response(self, request, code, data):
    request.write('HTTP/1.1 %d Maybe OK\n' % code)
    request.write('Content-Type: application/json\n')
    request.write('Content-Length: %s\n\n' % len(data))
    request.write(data)
    request.finish()

  def device_state(self):
    return 'idle'

  def get_common_info(self):
    return {
        'version': '3.0',
        'name': 'Sample Device',
        'device_state': self.device_state()
    }

  def post_session_cancel(self):
    if self.session_cancel_callback:
      self.session_cancel_callback.cancel()
    self.session_cancel_callback = self.CancelableClosure(self.session_cancel)
    self.ioloop.add_timeout(datetime.timedelta(minutes=2),
                            self.session_cancel_callback)

  def session_cancel(self):
    self.current_session = None

  # WifiHandler.Delegate implementation
  def on_wifi_connected(self, token):
    self.mdns_wrapper.set_setup_name(None)
    self.cloud_device.try_start(token)
    self.on_wifi = True

  def on_device_started(self):
    self.mdns_wrapper.set_id(self.cloud_device.get_device_id())

  def on_device_stopped(self):
    pass

  def stop(self):
    self.wifi_handler.stop()
    self.cloud_device.stop()


def main():
  state = State()
  state.load()

  ioloop = IOLoop.instance()

  handler = WebRequestHandler(ioloop, state)
  handler.start()
  def logic_stop():
    handler.stop()
  atexit.register(logic_stop)
  server = HTTPServer(handler.handle_request)
  server.listen(_DEVICE_PORT)

  ioloop.start()

if __name__ == '__main__':
  main()
