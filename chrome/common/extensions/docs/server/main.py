#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import cgi
import logging
import os
import re

from google.appengine.ext import webapp
from google.appengine.ext.webapp.util import run_wsgi_app
from google.appengine.api import memcache
from google.appengine.api import urlfetch

import app_known_issues

DEFAULT_CACHE_TIME = 300
VIEW_VC_ROOT = 'http://src.chromium.org'


class Channel():
  def __init__(self, name, tag):
    self.name = name
    self.tag = tag

Channel.DEV = Channel("dev", "2.0-dev")
Channel.BETA = Channel("beta", "1.1-beta")
Channel.STABLE = Channel("stable", "")
Channel.TRUNK = Channel("trunk", "")
Channel.CHANNELS = [Channel.TRUNK, Channel.DEV, Channel.BETA, Channel.STABLE]
Channel.DEFAULT = Channel.STABLE


def GetChannelByName(channelName):
  for channel in Channel.CHANNELS:
    if channel.name == channelName:
      return channel

  return None


def GetBranchRoot(branch):
  if branch is None:
    return '%s/viewvc/chrome/trunk/src/' % VIEW_VC_ROOT
  else:
    return '%s/viewvc/chrome/branches/%s/src/' % (VIEW_VC_ROOT, branch)


def GetExtensionsRoot(branch):
  return GetBranchRoot(branch) + 'chrome/common/extensions/'


def GetDocsRoot(branch, docFamily):
  if docFamily is None:
    return GetExtensionsRoot(branch) + 'docs/'
  else:
    return GetExtensionsRoot(branch) + ('docs/%s/' % docFamily)


def GetSrcUrl(branch, docFamily, path):
  # TODO(aa): Need cooler favicon.
  if path[0] == 'favicon.ico':
    return '%s/%s' % (VIEW_VC_ROOT, path[0])

  pathstr = '/'.join(path)

  if path[0] == 'api' and path[-1].endswith('.json'):
    return GetExtensionsRoot(branch) + pathstr

  if path[0] == 'third_party':
    if path[-1] == 'jstemplate_compiled.js':
      return GetBranchRoot(branch) + ('chrome/%s' % pathstr)
    else:
      return GetBranchRoot(branch) + pathstr

  return GetDocsRoot(branch, docFamily) + pathstr


def GetBranch(channel):
  """ Gets the name of the branch in source control that contains the code for
  the Chrome version currently being served on |channel|."""
  branch = memcache.get(channel.name)
  if branch is not None:
    return branch

  # query Omaha to figure out which version corresponds to this channel
  postdata = """<?xml version="1.0" encoding="UTF-8"?>
                <o:gupdate xmlns:o="http://www.google.com/update2/request"
                    protocol="2.0" testsource="crxdocs">
                <o:app appid="{8A69D345-D564-463C-AFF1-A69D9E530F96}"
                    version="0.0.0.0" lang="">
                <o:updatecheck tag="%s"
                    installsource="ondemandcheckforupdates" />
                </o:app>
                </o:gupdate>
                """ % channel.tag

  result = urlfetch.fetch(
      url="https://tools.google.com/service/update2",
      payload=postdata,
      method=urlfetch.POST,
      headers={'Content-Type':'application/x-www-form-urlencoded',
               'X-USER-IP': '72.1.1.1'})

  if result.status_code != 200:
    return None

  match = re.search(r'<updatecheck Version="\d+\.\d+\.(\d+)\.\d+"',
                    result.content)
  if match is None:
    logging.error("Cannot find branch for requested channel: " + result.content)
    return None

  branch = match.group(1)
  memcache.add(channel.name, branch, DEFAULT_CACHE_TIME)
  return branch

class MainPage(webapp.RequestHandler):
  def redirectToIndexIfNecessary(self):
    if len(self.path) > 0:
      return True
    newPath = self.request.path
    if not newPath.endswith('/'):
      newPath += '/'
    newPath += 'index.html'
    self.redirect(newPath)
    return False


  def initPath(self):
    self.path = self.request.path.split('/')

    # The first component is always empty.
    self.path.pop(0)

    # The last component might be empty if there was a trailing slash.
    if len(self.path) > 0 and self.path[-1] == '':
      self.path.pop()

    # Temporary hacks for apps.
    # TODO(aa): Remove once the apps content percolates through Chrome's release
    # process more.
    if self.path == ['apps'] or self.path == ['trunk', 'apps']:
      self.redirect('/trunk/apps/about_apps.html')
      return False

    # TODO(aa): Remove once we have a homepage for developer.chrome.com.
    if (self.path == [] and
        self.request.url.startswith('http://developer.chrome.com')):
      self.redirect('http://developers.google.com/chrome')
      return False

    # TODO(aa): Remove this soon.
    if (len(self.path) > 1 and self.path[1] == 'apps' and
        not self.request.url.startswith('http://localhost')):
      return False

    return self.redirectToIndexIfNecessary()


  def stripPathPrefix(self):
    # Remove chrome prefix if present. This happens in the case of
    # http://code.google.com/chrome/extensions/...
    if self.path[0:2] == ['chrome', 'extensions']:
      self.path = self.path[2:]
    return self.redirectToIndexIfNecessary()


  def initChannel(self):
    self.channel = GetChannelByName(self.path[0])
    if self.channel is not None:
      self.path.pop(0)
    else:
      self.channel = Channel.DEFAULT
    return self.redirectToIndexIfNecessary()


  def initDocFamily(self):
    if self.path[0] in ('extensions', 'apps'):
      self.docFamily = self.path[0]
      self.path.pop(0)
    else:
      self.docFamily = 'extensions'
    return self.redirectToIndexIfNecessary()


  def fetchContent(self):
    logging.info("fetching: %s" % str((self.branch, self.docFamily, self.path)))

    # For extensions, try the old directory layout first.
    result = None
    oldUrl = ''

    if self.docFamily == 'extensions':
      oldUrl = GetSrcUrl(self.branch, None, self.path)
      result = urlfetch.fetch(oldUrl)

    if result is None or result.status_code != 200:
      newUrl = GetSrcUrl(self.branch, self.docFamily, self.path)
      if oldUrl != newUrl:
        logging.info('Trying new directory layout...')
        result = urlfetch.fetch(newUrl)

    # Files inside of samples should be rendered with content-type
    # text/plain so that their source is visible when linked to. The only
    # types we should serve as-is are images.
    if (self.path[0] == 'examples' and
        not (result.headers['content-type'].startswith('image/') or
             result.headers['Content-Type'].startswith('image/'))):
      result.headers['content-type'] = 'text/plain'

    return result


  def get(self):
    if (not self.initPath() or
        not self.stripPathPrefix() or
        not self.initChannel() or
        not self.initDocFamily()):
      return

    cacheKey = str((self.channel.name, self.docFamily, self.path))
    result = memcache.get(cacheKey)
    if result is None:
      logging.info("cache miss: " + cacheKey)

      self.branch = None
      if self.channel is not Channel.TRUNK:
        self.branch = GetBranch(self.channel)

      result = self.fetchContent()
      memcache.add(cacheKey, result, DEFAULT_CACHE_TIME)

    if result is not None:
      for key in result.headers:
        self.response.headers[key] = result.headers[key]
      self.response.out.write(result.content)


application = webapp.WSGIApplication([
  ('/app_known_issues_snippet.html', app_known_issues.Handler),
  ('/.*', MainPage),
], debug=False)


def main():
  run_wsgi_app(application)


if __name__ == '__main__':
  main()
