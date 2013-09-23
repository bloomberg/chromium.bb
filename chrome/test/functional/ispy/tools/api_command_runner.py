# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""RunAPICommand function to authenticate with Ispy and run a command."""

import re
import requests


class ServerSideError(Exception):
  pass


class UnknownError(Exception):
  pass


def RunAPICommand(command, parameters, email, app_specific_password):
  """Performs the necessary authentication and runs an ispy api command.

  Args:
    command: The name of the command to run.
    parameters: A List of 2-tuples (parameter-name, parameter-value).
    email: A google.com email to connect to ispy with.
    app_specific_password: An application specific password registered
      to the given email account.

  Returns:
    A JSON representation of the result of the command.
  """
  app_name = 'google.com:ispy'
  base_url = 'http://ispy.googleplex.com/'
  # Use Requests to get an Auth key for the specified email/password.
  data = {'Email': email, 'Passwd': app_specific_password, 'service': 'ah',
          'source': app_name, 'accountType': 'GOOGLE'}
  auth_keys_text = requests.post('https://google.com/accounts/ClientLogin',
                                 data=data).text
  auth_keys = dict(line.split('=')
                   for line in auth_keys_text.split('\n') if line)
  # Establish a session by logging into _ah/login
  serv_args = {'continue': '', 'auth': auth_keys['Auth']}
  r2 = requests.get(base_url + '_ah/login',
                    params=serv_args, allow_redirects=False)
  r3 = requests.post(base_url+'run_command',
                     data=dict([('command', command)] + parameters.items()),
                     cookies=r2.cookies)
  try:
    return r3.json()
  except ValueError:
    if '<html>' in r3.text:
      match = re.match(r'^.+?<pre>(.+?)</pre>.+?$', r3.text, re.DOTALL)
      if match:
        raise ServerSideError(match.groups()[0])
      else:
        raise ServerSideError(r3.text)
    else:
      raise UnknownError(r3.text)
