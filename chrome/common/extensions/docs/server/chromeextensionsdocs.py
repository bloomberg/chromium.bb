#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import cgi
import logging
import re
import os

from google.appengine.ext import webapp
from google.appengine.ext.webapp.util import run_wsgi_app
from google.appengine.api import memcache
from google.appengine.api import urlfetch

# TODO(nickbaum): unit tests


# TODO(nickbaum): is this the right way to do constants?
class Channel():
  def __init__(self, name, tag):
    self.name = name
    self.tag = tag

  # TODO(nickbaum): unit test this
  def matchPath(self, path):
    match = "/" + self.name + "/"
    if path[0:len(match)] == match:
      return true
    else:
      return false

Channel.DEV = Channel("dev", "2.0-dev")
Channel.BETA = Channel("beta", "1.1-beta")
Channel.STABLE = Channel("stable", "")
Channel.CHANNELS = [Channel.DEV, Channel.BETA, Channel.STABLE]
Channel.TRUNK = Channel("trunk", "")
Channel.DEFAULT = Channel.STABLE


DEFAULT_CACHE_TIME = 300


class MainPage(webapp.RequestHandler):
  # get page from memcache, or else fetch it from src
  def get(self):
    path = os.path.realpath(os.path.join('/', self.request.path))
    # special path to invoke the unit tests
    # TODO(nickbaum): is there a less ghetto way to invoke the unit test?
    if path == "/test":
      self.unitTest()
      return
    # if root, redirect to index.html
    # TODO(nickbaum): this doesn't handle /chrome/extensions/trunk, etc
    if (path == "/chrome/extensions") or (path == "chrome/extensions/"):
      self.redirect("/chrome/extensions/index.html")
      return
    # else remove prefix
    if(path[:18] == "/chrome/extensions"):
      path = path[18:]
    # TODO(nickbaum): there's a subtle bug here: if there are two instances of the app,
    # their default caches will override each other. This is bad!
    result = memcache.get(path)
    if result is None:
      logging.info("Cache miss: " + path)
      url = self.getSrcUrl(path)
      if (url[1] is not Channel.TRUNK) and (url[0] != "http://src.chromium.org/favicon.ico"):
        branch = self.getBranch(url[1])
        url = url[0] % branch
      else:
        url = url[0]
      logging.info("Path: " + self.request.path)
      logging.info("Url: " + url)
      try:
        result = urlfetch.fetch(url)
        if result.status_code != 200:
          logging.error("urlfetch failed: " + url)
          # TODO(nickbaum): what should we do when the urlfetch fails?
      except:
        logging.error("urlfetch failed: " + url)
        # TODO(nickbaum): what should we do when the urlfetch fails?
      try:
        if not memcache.add(path, result, DEFAULT_CACHE_TIME):
          logging.error("Memcache set failed.")
      except:
        logging.error("Memcache set failed.")
    for key in result.headers:
      self.response.headers[key] = result.headers[key]
    self.response.out.write(result.content)

  def head(self):
    self.get()

  # get the src url corresponding to the request
  # returns a tuple of the url and the branch
  # this function is the only part that is unit tested
  def getSrcUrl(self, path):
    # from the path they provided, figure out which channel they requested
    # TODO(nickbaum) clean this logic up
    # find the first subdirectory of the path
    path = path.split('/', 2)
    url = "http://src.chromium.org/viewvc/chrome/"
    channel = None
    # if there's no subdirectory, choose the default channel
    # otherwise, figure out if the subdirectory corresponds to a channel
    if len(path) == 2:
      path.append("")
    if path[1] == "":
      channel = Channel.DEFAULT
      if(Channel.DEFAULT == Channel.TRUNK):
        url = url + "trunk/src/chrome/"
      else:
        url = url + "branches/%s/src/chrome/"
      path = ""
    elif path[1] == Channel.TRUNK.name:
      url = url + "trunk/src/chrome/"
      channel = Channel.TRUNK
      path = path[2]
    else:
      # otherwise, run through the different channel options
      for c in Channel.CHANNELS:
        if(path[1] == c.name):
          channel = c
          url = url + "branches/%s/src/chrome/"
          path = path[2]
          break
      # if the subdirectory doesn't correspond to a channel, use the default
      if channel is None:
        channel = Channel.DEFAULT
        if(Channel.DEFAULT == Channel.TRUNK):
          url = url + "trunk/src/chrome/"
        else:
          url = url + "branches/%s/src/chrome/"
        if path[2] != "":
          path = path[1] + "/" + path[2]
        else:
          path = path[1]
    # special cases
    # TODO(nickbaum): this is super cumbersome to maintain
    if path == "third_party/jstemplate/jstemplate_compiled.js":
      url = url + path
    elif path == "api/extension_api.json":
      url = url + "common/extensions/" + path
    elif path == "favicon.ico":
      url = "http://src.chromium.org/favicon.ico"
    else:
      if path == "":
        path = "index.html"
      url = url + "common/extensions/docs/" + path
    return [url, channel]

  # get the current version number for the channel requested (dev, beta or stable)
  # TODO(nickbaum): move to Channel object
  def getBranch(self, channel):
    branch = memcache.get(channel.name)
    if branch is None:
      # query Omaha to figure out which version corresponds to this channel
      postdata = """<?xml version="1.0" encoding="UTF-8"?>
                    <o:gupdate xmlns:o="http://www.google.com/update2/request" protocol="2.0" testsource="crxdocs">
                    <o:app appid="{8A69D345-D564-463C-AFF1-A69D9E530F96}" version="0.0.0.0" lang="">
                    <o:updatecheck tag="%s" installsource="ondemandcheckforupdates" />
                    </o:app>
                    </o:gupdate>
                    """ % channel.tag
      result = urlfetch.fetch(url="https://tools.google.com/service/update2",
                        payload=postdata,
                        method=urlfetch.POST,
                        headers={'Content-Type': 'application/x-www-form-urlencoded',
                                 'X-USER-IP': '72.1.1.1'})
      if result.status_code != 200:
        logging.error("urlfetch failed.")
        # TODO(nickbaum): what should we do when the urlfetch fails?
      # find branch in response
      match = re.search(r'<updatecheck Version="\d+\.\d+\.(\d+)\.\d+"', result.content)
      if match is None:
        logging.error("Version number not found: " + result.content)
        #TODO(nickbaum): should we fall back on trunk in this case?
      branch = match.group(1)
      # TODO(nickbaum): make cache time a constant
      if not memcache.add(channel.name, branch, DEFAULT_CACHE_TIME):
        logging.error("Memcache set failed.")
    return branch

  # TODO(nickbaum): is there a more elegant way to write this unit test?
  # I deliberately kept it dumb to avoid errors sneaking in, but it's so verbose...
  # TODO(nickbaum): should I break this up into multiple files?
  def unitTest(self):
    self.response.out.write("Testing TRUNK<br/>")
    self.check("/trunk/", "http://src.chromium.org/viewvc/chrome/trunk/src/chrome/common/extensions/docs/index.html", Channel.TRUNK)
    self.check("/trunk/index.html", "http://src.chromium.org/viewvc/chrome/trunk/src/chrome/common/extensions/docs/index.html", Channel.TRUNK)
    self.check("/trunk/getstarted.html", "http://src.chromium.org/viewvc/chrome/trunk/src/chrome/common/extensions/docs/getstarted.html", Channel.TRUNK)

    self.response.out.write("<br/>Testing DEV<br/>")
    self.check("/dev/", "http://src.chromium.org/viewvc/chrome/branches/%s/src/chrome/common/extensions/docs/index.html", Channel.DEV)
    self.check("/dev/index.html", "http://src.chromium.org/viewvc/chrome/branches/%s/src/chrome/common/extensions/docs/index.html", Channel.DEV)
    self.check("/dev/getstarted.html", "http://src.chromium.org/viewvc/chrome/branches/%s/src/chrome/common/extensions/docs/getstarted.html", Channel.DEV)

    self.response.out.write("<br/>Testing BETA<br/>")
    self.check("/beta/", "http://src.chromium.org/viewvc/chrome/branches/%s/src/chrome/common/extensions/docs/index.html", Channel.BETA)
    self.check("/beta/index.html", "http://src.chromium.org/viewvc/chrome/branches/%s/src/chrome/common/extensions/docs/index.html", Channel.BETA)
    self.check("/beta/getstarted.html", "http://src.chromium.org/viewvc/chrome/branches/%s/src/chrome/common/extensions/docs/getstarted.html", Channel.BETA)

    self.response.out.write("<br/>Testing STABLE<br/>")
    self.check("/stable/", "http://src.chromium.org/viewvc/chrome/branches/%s/src/chrome/common/extensions/docs/index.html", Channel.STABLE)
    self.check("/stable/index.html", "http://src.chromium.org/viewvc/chrome/branches/%s/src/chrome/common/extensions/docs/index.html", Channel.STABLE)
    self.check("/stable/getstarted.html", "http://src.chromium.org/viewvc/chrome/branches/%s/src/chrome/common/extensions/docs/getstarted.html", Channel.STABLE)

    self.response.out.write("<br/>Testing jstemplate_compiled.js<br/>")
    self.check("/trunk/third_party/jstemplate/jstemplate_compiled.js", "http://src.chromium.org/viewvc/chrome/trunk/src/chrome/third_party/jstemplate/jstemplate_compiled.js", Channel.TRUNK)
    self.check("/dev/third_party/jstemplate/jstemplate_compiled.js", "http://src.chromium.org/viewvc/chrome/branches/%s/src/chrome/third_party/jstemplate/jstemplate_compiled.js", Channel.DEV)
    self.check("/beta/third_party/jstemplate/jstemplate_compiled.js", "http://src.chromium.org/viewvc/chrome/branches/%s/src/chrome/third_party/jstemplate/jstemplate_compiled.js", Channel.BETA)
    self.check("/stable/third_party/jstemplate/jstemplate_compiled.js", "http://src.chromium.org/viewvc/chrome/branches/%s/src/chrome/third_party/jstemplate/jstemplate_compiled.js", Channel.STABLE)

    self.response.out.write("<br/>Testing extension_api.json<br/>")
    self.check("/trunk/api/extension_api.json", "http://src.chromium.org/viewvc/chrome/trunk/src/chrome/common/extensions/api/extension_api.json", Channel.TRUNK)
    self.check("/dev/api/extension_api.json", "http://src.chromium.org/viewvc/chrome/branches/%s/src/chrome/common/extensions/api/extension_api.json", Channel.DEV)
    self.check("/beta/api/extension_api.json", "http://src.chromium.org/viewvc/chrome/branches/%s/src/chrome/common/extensions/api/extension_api.json", Channel.BETA)
    self.check("/stable/api/extension_api.json", "http://src.chromium.org/viewvc/chrome/branches/%s/src/chrome/common/extensions/api/extension_api.json", Channel.STABLE)

    self.response.out.write("<br/>Testing favicon.ico<br/>")
    self.check("/trunk/favicon.ico", "http://src.chromium.org/favicon.ico", Channel.TRUNK)
    self.check("/dev/favicon.ico", "http://src.chromium.org/favicon.ico", Channel.DEV)
    self.check("/beta/favicon.ico", "http://src.chromium.org/favicon.ico", Channel.BETA)
    self.check("/stable/favicon.ico", "http://src.chromium.org/favicon.ico", Channel.STABLE)

    self.response.out.write("<br/>Testing DEFAULT<br/>")
    temp = Channel.DEFAULT
    Channel.DEFAULT = Channel.DEV
    self.check("/", "http://src.chromium.org/viewvc/chrome/branches/%s/src/chrome/common/extensions/docs/index.html", Channel.DEV)
    self.check("/index.html", "http://src.chromium.org/viewvc/chrome/branches/%s/src/chrome/common/extensions/docs/index.html", Channel.DEV)
    self.check("/getstarted.html", "http://src.chromium.org/viewvc/chrome/branches/%s/src/chrome/common/extensions/docs/getstarted.html", Channel.DEV)
    self.check("/third_party/jstemplate/jstemplate_compiled.js", "http://src.chromium.org/viewvc/chrome/branches/%s/src/chrome/third_party/jstemplate/jstemplate_compiled.js", Channel.DEV)
    self.check("/api/extension_api.json", "http://src.chromium.org/viewvc/chrome/branches/%s/src/chrome/common/extensions/api/extension_api.json", Channel.DEV)
    self.check("/css/ApiRefStyles.css", "http://src.chromium.org/viewvc/chrome/branches/%s/src/chrome/common/extensions/docs/css/ApiRefStyles.css", Channel.DEV)
    self.check("/favicon.ico", "http://src.chromium.org/favicon.ico", Channel.DEV)

    self.response.out.write("<br/>Testing DEFAULT (trunk)<br/>")
    Channel.DEFAULT = Channel.TRUNK
    self.check("/", "http://src.chromium.org/viewvc/chrome/trunk/src/chrome/common/extensions/docs/index.html", Channel.TRUNK)
    self.check("/index.html", "http://src.chromium.org/viewvc/chrome/trunk/src/chrome/common/extensions/docs/index.html", Channel.TRUNK)
    self.check("/getstarted.html", "http://src.chromium.org/viewvc/chrome/trunk/src/chrome/common/extensions/docs/getstarted.html", Channel.TRUNK)
    self.check("/third_party/jstemplate/jstemplate_compiled.js", "http://src.chromium.org/viewvc/chrome/trunk/src/chrome/third_party/jstemplate/jstemplate_compiled.js", Channel.TRUNK)
    self.check("/api/extension_api.json", "http://src.chromium.org/viewvc/chrome/trunk/src/chrome/common/extensions/api/extension_api.json", Channel.TRUNK)
    self.check("/css/ApiRefStyles.css", "http://src.chromium.org/viewvc/chrome/trunk/src/chrome/common/extensions/docs/css/ApiRefStyles.css", Channel.TRUNK)
    self.check("/favicon.ico", "http://src.chromium.org/favicon.ico", Channel.TRUNK)
    Channel.DEFAULT = temp

    return

  # utility function for my unit test
  # checks that getSrcUrl(path) returns the expected values
  # TODO(nickbaum): can this be replaced by assert or something similar?
  def check(self, path, expectedUrl, expectedChannel):
    actual = self.getSrcUrl(path)
    if (actual[0] != expectedUrl):
      self.response.out.write('<span style="color:#f00;">Failure:</span> path ' + path + " gave url " + actual[0] + "<br/>")
    elif (actual[1] != expectedChannel):
      self.response.out.write('<span style="color:#f00;">Failure:</span> path ' + path + " gave branch " + actual[1].name + "<br/>")
    else:
      self.response.out.write("Path " + path + ' <span style="color:#0f0;">OK</span><br/>')
    return


application = webapp.WSGIApplication([
  ('/.*', MainPage),
], debug=False)


def main():
  run_wsgi_app(application)


if __name__ == '__main__':
  main()
