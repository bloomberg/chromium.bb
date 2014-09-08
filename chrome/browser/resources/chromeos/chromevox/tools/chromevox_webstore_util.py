#!/usr/bin/env python

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''A set of utilities to interface with the Chrome Webstore API.'''

import SimpleHTTPServer
import SocketServer
import httplib
import json
import os
import re
import sys
import thread
import urllib
import webbrowser

PROJECT_ARGS = {
  'client_id': ('937534751394-gbj5334v9144c57qjqghl7d283plj5r4'
      '.apps.googleusercontent.com'),
  'grant_type': 'authorization_code',
  'redirect_uri': 'http://localhost:8000'
}

PORT = 8000

APP_ID = 'kgejglhpjiefppelpmljglcjbhoiplfn'
OAUTH_DOMAIN = 'accounts.google.com'
OAUTH_AUTH_COMMAND = '/o/oauth2/auth'
OAUTH_TOKEN_COMMAND = '/o/oauth2/token'
WEBSTORE_API_SCOPE = 'https://www.googleapis.com/auth/chromewebstore'

API_ENDPOINT_DOMAIN = 'www.googleapis.com'
COMMAND_GET_UPLOAD_STATUS = (
    '/chromewebstore/v1.1/items/%s?projection=draft' % APP_ID)
COMMAND_POST_PUBLISH = '/chromewebstore/v1.1/items/%s/publish' % APP_ID
COMMAND_POST_UPLOAD = '/upload/chromewebstore/v1.1/items/%s' % APP_ID

class CodeRequestHandler(SocketServer.StreamRequestHandler):
  def handle(self):
    content = self.rfile.readline()
    self.server.code = re.search('code=(.*) ', content).groups()[0]
    self.rfile.close()

def GetAuthCode():
  Handler = CodeRequestHandler
  httpd = SocketServer.TCPServer(("", PORT), Handler)
  query = '&'.join(['response_type=code',
                    'scope=%s' % WEBSTORE_API_SCOPE,
                    'client_id=%(client_id)s' % PROJECT_ARGS,
                    'redirect_uri=%(redirect_uri)s' % PROJECT_ARGS])
  auth_url = ' https://%s%s?%s' % (OAUTH_DOMAIN, OAUTH_AUTH_COMMAND, query)
  print 'Navigating to %s' % auth_url
  webbrowser.open(auth_url)
  httpd.handle_request()
  httpd.server_close()
  return httpd.code

def GetOauthToken(code, client_secret):
  PROJECT_ARGS['code'] = code
  PROJECT_ARGS['client_secret'] = client_secret
  body = urllib.urlencode(PROJECT_ARGS)
  conn = httplib.HTTPSConnection(OAUTH_DOMAIN)
  conn.putrequest('POST', OAUTH_TOKEN_COMMAND)
  conn.putheader('content-type', 'application/x-www-form-urlencoded')
  conn.putheader('content-length', len(body))
  conn.endheaders()
  conn.send(body)
  content = conn.getresponse().read()
  return json.loads(content)

def GetPopulatedHeader(client_secret):
  code = GetAuthCode()
  access_token = GetOauthToken(code, client_secret)
  url = 'www.googleapis.com'

  return {'Authorization': 'Bearer %(access_token)s' % access_token,
             'x-goog-api-version': 2,
             'Content-Length': 0
            }

def SendGetCommand(command, client_secret):
  headers = GetPopulatedHeader(client_secret)
  conn = httplib.HTTPSConnection(API_ENDPOINT_DOMAIN)
  conn.request('GET', command, '', headers)
  return conn.getresponse()

def SendPostCommand(command, client_secret, header_additions = {}, body=None):
  headers = GetPopulatedHeader(client_secret)
  headers = dict(headers.items() + header_additions.items())
  conn = httplib.HTTPSConnection(API_ENDPOINT_DOMAIN)
  conn.request('POST', command, body, headers)
  return conn.getresponse()

def GetUploadStatus(client_secret):
  '''Gets the status of a previous upload.
  Args:
    client_secret ChromeVox's client secret creds.
  '''
  return SendGetCommand(COMMAND_GET_UPLOAD_STATUS, client_secret)

# httplib fails to persist the connection during upload; use curl instead.
def PostUpload(file, client_secret):
  '''Posts an uploaded version of ChromeVox.
  Args:
    file A string path to the ChromeVox extension zip.
    client_secret ChromeVox's client secret creds.
  '''
  header = GetPopulatedHeader(client_secret)
  curl_command = ' '.join(['curl',
                           '-H "Authorization: %(Authorization)s"' % header,
                           '-H "x-goog-api-version: 2"',
                           '-X PUT',
                           '-T %s' % file,
                           '-v',
                           'https://%s%s' % (API_ENDPOINT_DOMAIN,
                                             COMMAND_POST_UPLOAD)])

  print 'Running %s' % curl_command
  if os.system(curl_command) != 0:
    sys.exit(-1)

def PostPublishTrustedTesters(client_secret):
  '''Publishes a previously uploaded ChromeVox extension to trusted testers.
  Args:
    client_secret ChromeVox's client secret creds.
  '''
  return SendPostCommand(COMMAND_POST_PUBLISH,
                         client_secret,
                         { 'publishTarget': 'trustedTesters'})

def PostPublish(client_secret):
  '''Publishes a previously uploaded ChromeVox extension publically.
  Args:
    client_secret ChromeVox's client secret creds.
  '''
  return SendPostCommand(COMMAND_POST_PUBLISH, client_secret)
